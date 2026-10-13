# Chromium Browser Process

**Mục lục:** role & process model · startup & main loop · threading model · core object model · navigation deep-dive · process management & security · servicification · Mojo from the browser side · shutdown & crashes · Interview Q&A

**Siblings:** [renderer_process.md](renderer_process.md) · [blink.md](blink.md) · [network_process.md](network_process.md)

---

## 1. Role: the Privileged "Kernel" of the Browser

The **browser process** is the first process started and the last to exit. Architecturally it plays the role of an OS kernel for the rest of Chromium:

- **Owns the UI** — windows, tabs, omnibox, menus, dialogs (Views/Aura on desktop, native UI on Mac/Android).
- **Coordinates all child processes** — spawns, monitors, and kills renderers, the GPU process, the network service, and utility processes.
- **Sole holder of powerful capabilities** — the filesystem, user data (cookies, passwords, history, `Profile` directory), OS integration, full network policy. Sandboxed children get these capabilities only *indirectly*, by asking the browser over IPC ("brokering").
- **Trust anchor** — it is the only process that is fully trusted. Every message arriving from a child process is treated as potentially hostile ("don't trust the renderer").

### Why multi-process at all?

| Goal | How the multi-process model delivers it |
| --- | --- |
| **Stability** | A crashed/leaking renderer kills one tab (sad tab), not the whole browser. |
| **Security** | Renderers run untrusted web content inside an OS **sandbox**; a compromised renderer still cannot touch the filesystem or arbitrary network — it must go through browser-side permission checks. |
| **Parallelism** | Independent sites render, JIT, and GC on different cores without blocking each other or the UI. |
| **Fault containment for drivers** | GPU drivers are notoriously crashy; isolating them in a GPU process lets the browser recover by relaunching it. |
| **Site Isolation** | Process = security boundary, so cross-site documents can be separated at the OS level (defense against Spectre-class attacks and renderer exploits). |

### Process model (modern desktop Chromium)

| Process | Count | Sandboxed? | Responsibility |
| --- | --- | --- | --- |
| **Browser** | exactly 1 | No (fully privileged) | UI, coordination, storage, policy, brokering |
| **Renderer** | many (per `SiteInstance` group; site-per-process on desktop) | Yes (tightest sandbox) | Blink + V8: parse, style, layout, paint, script. See [renderer_process.md](renderer_process.md) |
| **GPU** | 1 (relaunched on crash) | Yes (GPU sandbox) | Viz display compositor, OOP rasterization, WebGL/WebGPU command execution |
| **Network service** | 1 utility process (in-process on Android) | Yes (on most platforms) | All sockets/HTTP/DNS/cookies via `services/network` |
| **Utility** | on demand | Yes (per-service policy) | Short- or long-lived services: audio, data decoder, storage, unzipping, printing… |
| **Zygote (Linux)** | 1 | pre-sandbox template | Forks renderers cheaply with libraries pre-loaded; enables sandbox setup before any web content runs (`docs/linux/zygote.md`) |

The set of documents that must share a renderer is decided by the browser using `SiteInstance` (see §4 and §6); on desktop the default is **full Site Isolation** — each renderer is *locked* to one site (`docs/process_model_and_site_isolation.md`).

```
                    ┌────────────────────────────┐
                    │      BROWSER PROCESS       │
                    │  UI · profiles · policy    │
                    │  (the only trusted proc)   │
                    └─┬──────┬──────┬──────┬─────┘
              Mojo IPC│      │      │      │
        ┌─────────────┘      │      │      └───────────────┐
        ▼                    ▼      ▼                      ▼
 ┌──────────────┐   ┌──────────────┐  ┌───────────────┐ ┌───────────────┐
 │ Renderer #1  │   │ Renderer #2  │  │  GPU process  │ │ Utility procs │
 │ site A       │   │ site B       │  │ Viz compositor│ │ network svc,  │
 │ Blink + V8   │   │ Blink + V8   │  │ OOP raster    │ │ storage, audio│
 └──────────────┘   └──────────────┘  └───────────────┘ └───────────────┘
     sandboxed          sandboxed         sandboxed         sandboxed
```

---

## 2. Startup & the Main Loop

Chromium is layered: **`content/`** implements the multi-process web platform runtime; **`chrome/`** is one *embedder* of content (others: WebView, Electron-style embedders, headless). The seam between them is the **Content API** (`content/public/`).

Startup call chain (browser process):

```
chrome/app/chrome_main.cc            (embedder entry point, ChromeMainDelegate)
        │
        ▼
ContentMain()                        content/app/content_main.cc
        │
        ▼
ContentMainRunnerImpl                content/app/content_main_runner_impl.cc
        │   • base initialization, feature list, Mojo core init
        │   • dispatch on --type= switch:  no --type → browser process
        ▼
BrowserMainRunnerImpl                content/browser/browser_main_runner_impl.cc
        │
        ▼
BrowserMainLoop                      content/browser/browser_main_loop.cc
        • CreateThreads(): UI thread becomes BrowserThread::UI,
          spawns the IO thread (BrowserThread::IO), starts base::ThreadPool
        • brings up Mojo IPC support, GPU data manager, network service, …
        • calls into the embedder via BrowserMainParts
        • RunMainMessageLoop() — runs until shutdown
```

Child processes enter through the *same* `ContentMain()`; the `--type=renderer|gpu-process|utility` command-line switch selects which `*Main()` function runs (e.g. `RendererMain`). This is why every Chromium process shows the same executable name.

### The embedder API: `ContentBrowserClient`

`content/public/browser/content_browser_client.h` is the biggest embedder hook. Chrome implements it as `ChromeContentBrowserClient` (`chrome/browser/chrome_content_browser_client.cc`). Content calls out through it for every policy decision it does not want to hard-code:

```cpp
// Simplified — real interface has hundreds of virtuals.
class ContentBrowserClient {
 public:
  virtual std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(...);
  virtual void AppendExtraCommandLineSwitches(base::CommandLine*, int child_pid);
  virtual bool ShouldUseProcessPerSite(BrowserContext*, const GURL& site_url);
  virtual void CreateThrottlesForNavigation(NavigationThrottleRegistry&);
      // embedders call registry.AddThrottle(...)
  virtual void RegisterBrowserInterfaceBindersForFrame(...);  // Mojo caps
};
```

Interview takeaway: **content/ must never depend on chrome/**. Anything Chrome-specific (extensions, Safe Browsing, prefs UI) is injected via `ContentBrowserClient`, `NavigationThrottle`s, and Mojo interface registration.

---

## 3. Threading Model

The browser process has **two special named threads plus a general-purpose pool** (`docs/threading_and_tasks.md`):

| Thread | Name in code | Purpose | Blocking allowed? |
| --- | --- | --- | --- |
| Main thread | `BrowserThread::UI` | All UI, and almost all `content/browser` objects (`WebContents`, `RenderProcessHost`, `NavigationRequest`, profiles) live here | **Never** — jank = dropped frames on browser UI |
| IO thread | `BrowserThread::IO` | Receives Mojo/IPC messages from child processes and dispatches them (mostly hopping them to UI or pool). Despite the name it does **no disk I/O** | **Never** — blocking it stalls all IPC for every child |
| Thread pool | `base::ThreadPool` | Everything blocking or CPU-heavy: file I/O, SQLite, zip, hashing, proxy resolution glue… | Yes (with `base::MayBlock()` trait) |

`BrowserThread` is defined in `content/public/browser/browser_thread.h`. Modern code does not "create threads"; it acquires **task runners**:

```cpp
// Post to the UI thread (content-layer helper in browser_thread.h):
content::GetUIThreadTaskRunner({})->PostTask(
    FROM_HERE, base::BindOnce(&Tab::UpdateTitle, tab, title));

// Blocking work goes to the pool, on a SEQUENCE (not a dedicated thread):
scoped_refptr<base::SequencedTaskRunner> db_runner =
    base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

db_runner->PostTaskAndReplyWithResult(
    FROM_HERE,
    base::BindOnce(&ReadHistoryFromDisk, path),   // runs on pool, may block
    base::BindOnce(&HistoryUI::Show, weak_this)); // reply on calling sequence
```

Key idioms a senior candidate should articulate:

- **Sequences over threads.** A `base::SequencedTaskRunner` guarantees ordered, non-concurrent execution without pinning a physical thread — thread-safety by *sequence affinity*, checked with `SEQUENCE_CHECKER`.
- **The "never block the UI thread" rule** is enforced culturally and mechanically: `base::ScopedBlockingCall` asserts if blocking primitives run on UI/IO threads.
- **The IO thread is an IPC pump.** Handlers must be O(µs): deserialize, validate, re-post. Anything else starves every child process's messages.
- Cross-thread object access is avoided; instead, state is owned by one sequence and mutated via posted tasks (`base::WeakPtr` for cancellation-on-destruction).

```
   BROWSER PROCESS
   ┌───────────────────────────────────────────────────────────┐
   │  UI thread (BrowserThread::UI)                            │
   │   WebContents, RenderProcessHost, NavigationRequest, UI   │
   ├───────────────────────────────────────────────────────────┤
   │  IO thread (BrowserThread::IO)                            │
   │   Mojo message pump — receive, validate, re-dispatch      │
   ├───────────────────────────────────────────────────────────┤
   │  base::ThreadPool (N workers)                             │
   │   [seq A: history DB] [seq B: cache] [parallel: hashing]  │
   └───────────────────────────────────────────────────────────┘
```

---

## 4. Core Object Model

The ownership chain every browser engineer must know (all UI-thread objects):

```
BrowserContext (content)  ≈  Profile (chrome/)          — "who is browsing"
 └── StoragePartition(s)  content/browser/storage_partition_impl.cc
      └── cookies, cache, storage; owns browser-side URLLoaderFactory access
WebContents                                              — "a tab's contents"
 ├── NavigationController  content/browser/renderer_host/navigation_controller_impl.cc
 │        session history (back/forward list) for the primary frame tree
 ├── FrameTree(s)          content/browser/renderer_host/frame_tree.cc
 │    └── FrameTreeNode → RenderFrameHost (RFH)          — one per frame
 └── observers: WebContentsObserver, WebContentsDelegate (embedder UI hooks)

RenderProcessHost  content/browser/renderer_host/render_process_host_impl.cc
     browser-side handle to ONE renderer process (1:1), owns its Mojo channel
SiteInstance       content/browser/site_instance_impl.cc
     security principal grouping; maps documents → RenderProcessHost
```

- **`BrowserContext`** (`content/public/browser/browser_context.h`) is content's notion of a browsing session; Chrome subclasses it as `Profile`. Incognito is simply a second, ephemeral `BrowserContext`.
- **`WebContents`** (`content/public/browser/web_contents.h`) is *the* embedding API for "a web page and everything needed to display it". A tab, a `<webview>`, a DevTools window — each is a `WebContents`.
- **`RenderProcessHost` ↔ renderer process.** All browser-side communication with a renderer flows through its RPH: process launch, priority, shutdown, the Mojo channel, and per-process security state. Multiple frames/tabs may share one RPH (process reuse).
- **`RenderFrameHost` (RFH)** (`content/browser/renderer_host/render_frame_host_impl.cc`) is the browser-side peer of one *document* in one frame; its renderer-side twin is `RenderFrameImpl`. With **OOPIFs**, one page's frame tree spans multiple processes, so a `WebContents` can be backed by several RPHs at once. (`RenderViewHost`/`RenderView` are legacy remnants of the page-based model — mention them only as history.)

### MPArch: one WebContents, many FrameTrees

Since the **Multiple Page Architecture** (MPArch) refactor, a `WebContents` no longer equals a single frame tree (`docs/frame_trees.md`):

| FrameTree kind | Why it exists |
| --- | --- |
| **Primary** | What the user sees; the one the `NavigationController` shows |
| **Prerender** | `speculationrules` prerendering loads a full hidden page, then *activates* it by swapping it into primary |
| **Fenced frame** | An embedded but strongly isolated page gets its own *inner* frame tree |
| **BFCache pages** | Not a separate tree, but non-primary *pages*: the old page's RFH subtree is kept frozen and can be restored instantly |

Consequently RFHs have a **lifecycle state** (`kSpeculative`, `kPrerendering`, `kActive`, `kInBackForwardCache`, `kPendingDeletion`) and code must never assume "the" main frame — hence APIs like `WebContents::GetPrimaryMainFrame()`.

### SiteInstance, process reuse, spare process

A **`SiteInstance`** (`content/public/browser/site_instance.h`) represents "documents of one site principal inside one group of related windows (`BrowsingInstance`)". The browser maps SiteInstances to processes under policies described in `docs/process_model_and_site_isolation.md`:

- Desktop default: **site-per-process** — the process is *locked* to one site (see `ProcessLock`, §6).
- A soft **process limit** (derived from RAM) makes Chromium start reusing same-site processes rather than growing unboundedly; OOPIF subframes reuse same-site processes aggressively.
- `ShouldUseProcessPerSite` (embedder hook) can force one-process-per-site across all windows (used for extensions, New Tab Page).
- **Spare `RenderProcessHost`** (`content/browser/renderer_host/spare_render_process_host_manager_impl.cc`): the browser keeps one warm, *unlocked* renderer pre-launched so the next navigation skips process startup latency (tens to hundreds of ms). On use, it gets locked to the target site and a new spare is warmed.

---

## 5. Navigation Deep-Dive: `NavigationRequest`

Navigation is **browser-initiated by design** (historically "PlzNavigate" — today it is simply *the* model). The renderer never fetches a navigation itself; it *asks* the browser, and the browser owns the whole lifecycle in **`NavigationRequest`** (`content/browser/renderer_host/navigation_request.cc`), surfaced publicly as `NavigationHandle` (`content/public/browser/navigation_handle.h`). Reference: `docs/navigation.md`.

Why browser-side navigation matters (great interview answer material):

1. **Security** — the process/site decision must be made by trusted code *before* content is handed to a renderer; a compromised renderer cannot navigate itself to `chrome://settings`.
2. **Correct process selection** — the final URL (after redirects!) and response headers (COOP, CSP sandbox, `Content-Disposition`) determine the right process; only the browser sees all of that.
3. **Features** — throttling/canceling (Safe Browsing, extensions' webRequest), downloads vs. navigations, prerender activation — all need a central, trusted choke point.

### Lifecycle

```
 RENDERER (old doc)          BROWSER (UI thread)                 NETWORK SERVICE        RENDERER (new doc)
 ──────────────────          ───────────────────                 ───────────────        ──────────────────
  link click
    │ mojom::FrameHost::BeginNavigation
    ├──────────────────────►│
    │                       │ create NavigationRequest
    │                       │ (or: omnibox → NavigationController
    │                       │        ::LoadURLWithParams → same path)
    │◄──────────────────────┤ run beforeunload in current RFH
    │  dialog? proceed/stop │
    │                       │ NavigationThrottles: WillStartRequest
    │                       │ pick/create speculative RFH (SiteInstance!)
    │                       │        │ URLLoaderFactory::CreateLoaderAndStart
    │                       │        ├──────────────────────►│
    │                       │        │   redirects ◄─────────┤  (each redirect:
    │                       │  WillRedirectRequest throttles │   re-check site,
    │                       │        │                       │   maybe new RFH)
    │                       │        │   response head ◄─────┤
    │                       │ WillProcessResponse throttles
    │                       │ FINAL RFH decision (RenderDocument:
    │                       │   cross-document nav → fresh RFH)
    │                       │ mojom::NavigationClient::CommitNavigation
    │                       │   (+ URLLoaderClient endpoints to stream body)
    │                       ├────────────────────────────────────────────────►│
    │                       │                                                 │ create document,
    │                       │       DidCommitProvisionalLoad (ack)            │ Blink starts parsing
    │                       │◄────────────────────────────────────────────────┤
    │                       │ NavigationController commits entry;
    │                       │ old RFH → BFCache or pending deletion
    │                       │ (unload/pagehide run after swap)
```

Key details to be able to defend:

- **beforeunload** runs *first*, in the old document's renderer; the browser waits for the answer (with a timeout) before touching the network.
- The request goes out through a **browser-created `URLLoaderFactory`** obtained from the `StoragePartition`, parameterized with trusted info (initiator, `net::IsolationInfo` for cache/cookie partitioning). The renderer never holds the bytes until commit.
- **The final RFH is chosen after the response** — `RenderFrameHostManager` (`content/browser/renderer_host/render_frame_host_manager.cc`) computes the target `SiteInstance` from the *final* URL + headers, possibly discarding the speculative RFH created earlier.
- **RenderDocument** (`docs/render_document.md`): every cross-document navigation commits into a **new** `RenderFrameHost`, even same-site/same-process — RFH lifetime == document lifetime. This removed a huge class of "reused RFH" state bugs. (Historically same-site navs reused the RFH.)
- **Commit + ack**: the browser sends `CommitNavigation` on the `NavigationClient` interface (`content/common/navigation_client.mojom`); the renderer replies with `DidCommitProvisionalLoad`. Until the ack, the navigation is "pending commit" — a delicate window interview questions love (what if the renderer crashes here? The speculative RFH is cleaned up and the navigation fails without corrupting the current page).
- **Same-document navigations** (`pushState`, fragment) skip network and RFH swap entirely; the renderer informs the browser after the fact — one of the few renderer-initiated commits, heavily validated browser-side.
- **NavigationThrottles** are the sanctioned embedder interception point (deferring/canceling); Chrome registers Safe Browsing, policy, and extension throttles via `ContentBrowserClient::CreateThrottlesForNavigation`.

---

## 6. Process Management & Security: the Browser as Reference Monitor

### ChildProcessSecurityPolicy

`content/public/browser/child_process_security_policy.h`, implemented in `content/browser/security/cpsp/child_process_security_policy_impl.cc` (recently moved under `security/cpsp/`; a Rust port lives alongside the C++). It is a singleton **reference monitor**: a table of *grants* per child process ID.

```cpp
// Simplified usage pattern inside a browser-side IPC handler:
void RenderFrameHostImpl::OnOpenFile(const base::FilePath& path, ...) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadFile(GetProcess()->GetDeprecatedID(), path)) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_FILE_ACCESS_DENIED);
    return;  // renderer is terminated — it asked for something never granted
  }
  // ... broker the privileged operation on the child's behalf
}
```

- Grants are made *before* capability use: e.g. when the user picks a file in a file dialog, the browser calls `GrantReadFile(child_id, path)`; only then may the renderer read it (through the browser/network layers — the sandbox still blocks direct `open()`).
- Checks: `CanRequestURL`, `CanCommitURL`, `CanReadFile`, `CanAccessDataForOrigin` — the last one is the workhorse of Site Isolation, consulted on cookie access, storage IPCs, `postMessage` routing, etc.
- **Failure policy:** an out-of-grant request is not an error to report back — it is proof the renderer is compromised or buggy. `bad_message::ReceivedBadMessage()` (`content/browser/bad_message.h`) logs a histogram and **kills the process**.

### ProcessLock & site isolation decisions

A `ProcessLock` records what a `RenderProcessHost` is allowed to host — typically one site (`https://example.com`), sometimes a whole scheme (`file://`), or "unlocked" where full isolation is off (Android partial isolation). The lock is set when the first document commits and can only tighten. Every subsequent commit and data-access check is validated against it: *"is this process allowed to have data for origin O?"* This is the browser-side half of Site Isolation; the renderer-side half (CORB/ORB, origin checks in Blink) is defense in depth, not the boundary.

### Sandbox brokering

The sandbox (see `sandbox/`, policies per process type) removes ambient authority from children: no filesystem, no raw sockets, no window handles. Anything privileged becomes a **brokered operation**:

| Child wants | Browser-side broker |
| --- | --- |
| Read a user file | File chooser UI + `GrantReadFile` + file ops brokered via Mojo |
| Cookies for its site | `RestrictedCookieManager` bound per-frame with browser-stamped origin |
| Fetch a URL | `URLLoaderFactory` pre-bound with trusted params (§7) |
| Fonts, clipboard, GPU | Dedicated brokered Mojo services with per-interface checks |

The golden rule: **parameters that carry authority (origins, site URLs, process IDs) are never taken from the renderer's message; the browser stamps them into the Mojo endpoint at bind time**, so a compromised renderer cannot lie about who it is.

On Linux, the **zygote** (`docs/linux/zygote.md`) makes this workable: renderers are forked from a pre-warmed template *before* entering the sandbox, so they never need filesystem access to load their own libraries.

---

## 7. Servicification: the Browser as Service Coordinator

Chromium decomposed monolithic browser-process subsystems into **Mojo services** (`docs/mojo_and_services.md`) that the browser process *coordinates* but does not necessarily *host*:

| Service | Code | Process placement | Notes |
| --- | --- | --- | --- |
| **Network Service** | `services/network/` | Utility process (in-process on Android) | All HTTP/sockets/cookies/DNS. Sandboxed on most platforms. |
| **Storage Service** | `components/services/storage/` | Browser process (designed to be movable) | Quota, LocalStorage, IndexedDB, ServiceWorker storage backends |
| **Device Service** | `services/device/` | Browser process | USB/HID/Bluetooth/sensors brokering |
| **Data Decoder** | `services/data_decoder/` | Utility, often per-decode | Parses untrusted data (JSON, images, XML) *outside* the browser |
| **Audio** | `services/audio/` | Utility process | Moved out for stability + sandboxing of OS audio stacks |
| **Viz / GPU** | `services/viz/`, GPU process | GPU process | Display compositor + OOP raster; browser talks via `content/browser/gpu/gpu_process_host.cc` |

### Network Service in one breath

`services/network/README.md`: the browser holds the **trusted** interfaces — `network.mojom.NetworkService` (global) and one `network.mojom.NetworkContext` per `StoragePartition` (owns the cookie jar, cache, socket pools). These never go to a renderer. What renderers get is a **`URLLoaderFactory`**:

```
Browser: NetworkContext::CreateURLLoaderFactory(params) where params include:
   • process_id                      — which child may use it
   • request_initiator_origin_lock   — the origin it may claim as initiator
   • isolation_info / trust tokens   — cache & cookie partitioning keys
   • is_trusted = false              — renderer factories can't override isolation
        │
        ▼  (pending_remote<URLLoaderFactory> sent to the renderer, per frame)
Renderer: factory->CreateLoaderAndStart(request, client)   — for subresources
```

Because the factory is **pre-parameterized by the browser per frame/origin**, the network service can enforce site isolation and cookie policy without believing anything the renderer says. Navigations, by contrast, use browser-owned trusted factories (`content/browser/storage_partition_impl.cc` provides them and reconnects if the network service crashes — the browser just restarts it and rebuilds contexts; renderers observe transient fetch failures rather than a browser crash).

The GPU process relationship is similar in spirit: renderers produce compositor frames (cc, see [renderer_process.md](renderer_process.md)) and submit them to the **Viz** display compositor in the GPU process; the browser also submits its UI frames to Viz, and Viz aggregates all surfaces into the final display frame. The browser's job is lifecycle (launch, crash-relaunch, GPU feature blocklisting via `GpuDataManager`), not pixels.

---

## 8. Mojo from the Browser Side

(Read `mojo/README.md` for Mojo itself; here: how the *browser* hands out capabilities.)

### BrowserInterfaceBroker: the capability gate

Each frame (and each worker) gets exactly one **`BrowserInterfaceBroker`** pipe (`third_party/blink/public/mojom/browser_interface_broker.mojom`) with a single method: `GetInterface(mojo.GenericPendingReceiver)`. Browser-side, `BrowserInterfaceBrokerImpl` (`content/browser/browser_interface_broker_impl.h`) looks the interface name up in a **binder map** populated in `content/browser/browser_interface_binders.cc` (+ embedder additions via `ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame`):

```cpp
// content/browser/browser_interface_binders.cc (simplified)
void PopulateFrameBinders(RenderFrameHostImpl* host, mojo::BinderMap* map) {
  map->Add<device::mojom::VibrationManager>(...);
  map->Add<blink::mojom::NotificationService>(
      base::BindRepeating(&RenderFrameHostImpl::CreateNotificationService,
                          base::Unretained(host)));  // host outlives map
  // Each binder can consult permissions, feature flags, and the frame's
  // committed origin BEFORE binding — this is the capability check.
}
```

Design points worth stating in an interview:

- **Capability model:** a renderer can only obtain interfaces its context was *registered* for; requesting an unknown/forbidden interface is a `ReceivedBadMessage` kill. Per-frame brokers mean capabilities are scoped to the frame's *committed origin*, not the process.
- **MPArch integration:** `BrowserInterfaceBrokerImpl` supports a `MojoBinderPolicyApplier` that *defers or denies* binding for prerendering pages (a prerendered page must not vibrate, show notifications, etc. until activation).
- Frame ↔ browser core channels aren't fetched via the broker; they are established at frame creation (`mojom::Frame` / `mojom::FrameHost` in `content/common/frame.mojom`) as **associated interfaces**.

### Associated vs. non-associated interfaces

| | Non-associated | Associated |
| --- | --- | --- |
| Transport | Own message pipe | Multiplexed onto an existing pipe (for frames: the channel shared with legacy/`FrameHost` traffic) |
| Ordering | **No** ordering guarantee vs. other interfaces | FIFO with everything else on that pipe |
| Threading | Can bind/dispatch on any thread (e.g. straight to a ThreadPool sequence — no UI-thread hop) | Dispatched where the master pipe is dispatched |
| Use when | Independent subsystem (metrics, media remoting) | Message order vs. navigation/commit matters (e.g. must arrive *before* `DidCommitProvisionalLoad`) |

Classic bug class: making an interface non-associated when its messages race against commit — e.g. a "set history offset" message arriving after the new document committed. If ordering with navigation matters, it must be **associated** (or redesigned to be idempotent/stamped with a token like `NavigationToken`).

---

## 9. Shutdown & Crash Handling

### Renderer crash → sad tab

The browser learns of child death via the process launcher/watcher plumbed into `RenderProcessHostImpl`:

```cpp
class MyFeature : public content::RenderProcessHostObserver {
  void RenderProcessExited(content::RenderProcessHost* host,
                           const content::ChildProcessTerminationInfo& info)
      override {
    // info.status: NORMAL_TERMINATION / ABNORMAL / CRASHED / OOM / KILLED
  }
};
// Tab-level: WebContentsObserver::PrimaryMainFrameRenderProcessGone()
//  → Chrome swaps in the "Aw, Snap!" sad-tab UI; reload spawns a new
//    renderer and re-navigates. The browser and other tabs are unaffected.
```

Crash dumps go through Crashpad; the RPH object itself typically survives and can be reinitialized (`Init()`) for reload. GPU-process crashes are auto-relaunched a bounded number of times before falling back (software rendering); network-service crashes trigger transparent restart + `NetworkContext` reconstruction (§7).

### Fast shutdown

On browser exit, waiting for every renderer to run `unload` handlers would be slow. `RenderProcessHostImpl::FastShutdownIfPossible()` lets the browser **outright terminate** a renderer when it is safe: no unload/beforeunload handlers registered, no pending navigations, nothing (like an in-flight `keepalive` fetch) that must complete. This is why a well-behaved page without unload handlers makes the whole browser quit faster — and why `beforeunload`/`unload` are being replaced by `pagehide`/`visibilitychange` in web guidance.

Browser-process shutdown itself is phased (profiles flush on ThreadPool with `BLOCK_SHUTDOWN` traits for critical data like cookies; `SKIP_ON_SHUTDOWN` for droppable work) — a good example of why `TaskShutdownBehavior` traits exist.

---

## Interview Q&A

**Q1. Why does Chromium use multiple processes instead of threads, given the memory cost?**
Processes give an OS-enforced boundary that threads cannot: (1) *fault isolation* — a renderer crash or OOM kills a tab, not the browser; (2) *security* — untrusted web content runs in a sandboxed process with no ambient authority, so an exploited renderer must still get past browser-side permission checks (`ChildProcessSecurityPolicy`) to do damage; (3) *Site Isolation* — cross-site data can be kept out of an attacker's address space entirely, which is the only robust answer to Spectre-style speculative reads; (4) parallelism across cores. The memory cost is managed with process reuse policies, a soft process limit scaled to RAM, and partial isolation on low-end Android.

**Q2. What may run on the browser IO thread, and why is blocking it worse than blocking a pool thread?**
The IO thread is the Mojo/IPC dispatch pump for *all* child processes. Handlers must only deserialize, validate, and re-post work. Blocking it stalls message delivery browser-wide — every renderer's IPCs (input acks, navigation commits, sync interface calls) queue up, which manifests as whole-browser jank, not one feature being slow. Blocking work belongs on `base::ThreadPool` with `base::MayBlock()`, usually behind a `SequencedTaskRunner` for state affinity.

**Q3. Walk me through what happens between typing a URL and pixels, from the browser process's perspective.**
Omnibox → `NavigationController::LoadURLWithParams` → `NavigationRequest` created on the UI thread. Beforeunload runs in the old RFH. Throttles run (`WillStartRequest`), a speculative RFH in the right `SiteInstance` is created (often from the spare process), and the request is issued through a trusted `URLLoaderFactory` into the network service. After redirects and response headers arrive, throttles run again and the *final* RFH/process choice is made (final URL + COOP etc.; RenderDocument means a fresh RFH for any cross-document nav). The browser sends `CommitNavigation` with the response body pipe; the renderer builds the document and acks with `DidCommitProvisionalLoad`; the browser commits the `NavigationEntry` and puts the old RFH into BFCache or pending deletion. Rendering then proceeds in the renderer (Blink/cc) with frames submitted to Viz in the GPU process — see [renderer_process.md](renderer_process.md) and [blink.md](blink.md).

**Q4. Why is navigation browser-initiated (PlzNavigate)? What broke with renderer-initiated navigation?**
With renderer-driven navigation, the process decision had to be made before the response was seen, and trusted state (what's being fetched, where it will commit) lived in an untrusted process. Browser-side `NavigationRequest` fixes: correct process selection after redirects/headers, a single trusted choke point for throttling/Safe Browsing/downloads-vs-navigations, and the invariant that a compromised renderer cannot commit itself anywhere it likes — every commit is validated (`CanCommitURL`, `ProcessLock`) and lies are answered with process termination.

**Q5. What is a SiteInstance vs. a BrowsingInstance vs. a ProcessLock?**
A *BrowsingInstance* is a group of windows/frames that can reach each other by script (`window.opener` chains); within it, same-site documents must be able to script synchronously, so they are grouped into a *SiteInstance* — the unit mapped to a process. A *ProcessLock* is the enforcement record on the `RenderProcessHost`: which site (or scheme) the process may host; `CanAccessDataForOrigin` checks against it on every sensitive IPC. SiteInstance decides placement; ProcessLock enforces it after the fact.

**Q6. The renderer sends an IPC asking for the cookies of another site. What happens, step by step?**
The Mojo message arrives on the IO thread and is dispatched to the browser-side handler (or directly to the network service). The origin used is *not* taken from the message: cookie access flows through a `RestrictedCookieManager` bound per-frame with the browser-stamped origin, and browser-side checks call `ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin(child_id, origin)` against the `ProcessLock`. A mismatch is treated as a compromised renderer: `bad_message::ReceivedBadMessage()` kills the process. No error is returned to the caller — you don't negotiate with an exploited process.

**Q7. What is the spare RenderProcessHost and what are the trade-offs?**
A pre-launched, sandbox-initialized renderer with no site lock, kept warm by `SpareRenderProcessHostManager`. First navigation needing a new process claims it, cutting process-startup latency out of navigation. Trade-offs: ~1 idle process worth of memory, and it must be discarded/mismatched cases handled (wrong StoragePartition, need for a locked-at-launch process). It's a classic latency-for-memory trade, tuned differently on Android.

**Q8. Explain associated vs. non-associated Mojo interfaces with a concrete ordering bug.**
Associated interfaces share an existing pipe and preserve FIFO order with everything on it; non-associated interfaces have their own pipe and *no* cross-interface ordering. Bug: send "update session history state" on a standalone pipe while `CommitNavigation`/`DidCommitProvisionalLoad` travel on the frame channel — the update can arrive after a new document committed and clobber the wrong entry. Fix: make it associated with the frame channel, or make the message idempotent by stamping it with a navigation token. Rule of thumb: if a message's meaning depends on "which document is current", it must be ordered with commit.

**Q9. How does the browser process survive a network service crash? A GPU crash?**
Both are designed-for events. Network: the browser owns `NetworkService`/`NetworkContext` remotes; on disconnect it restarts the utility process and rebuilds contexts (e.g. `StoragePartitionImpl`'s factory getters detect disconnection and reconnect); in-flight `URLLoader`s error out and consumers retry. GPU: `GpuProcessHost` relaunches the process; renderers' Viz/raster contexts are lost and recreated, surfaces resubmitted; after repeated crashes Chromium falls back (software compositing / blocklist entries). Only the *browser* process is unrecoverable — that's why it holds no web content.

**Q10. What is RenderDocument and why was it worth a multi-year refactor?**
Historically a same-site navigation reused the existing `RenderFrameHost`, so an RFH could outlive documents, and every feature had to reset per-document state correctly on reuse — an endless bug/security-bug source (state from document A observable in document B). RenderDocument makes RFH lifetime == document lifetime: every cross-document navigation commits into a new RFH. Together with MPArch (prerender/BFCache pages as non-primary pages with real RFHs) it gives one uniform rule: never cache a raw RFH pointer across navigation; key state by document.

**Q11. Where would Chrome-the-product hook "block this navigation if it's phishing" without patching content/?**
Via the Content API: `ChromeContentBrowserClient::CreateThrottlesForNavigation` registers a `NavigationThrottle` whose `WillStartRequest`/`WillProcessResponse` can `DEFER` (async Safe Browsing check) then `CANCEL` and show an interstitial. This layering — content/ owns mechanism, embedder injects policy via `ContentBrowserClient` — is the answer template for any "where does Chrome-specific logic go?" question.

**Q12. Why does the browser process do permission checks on *every* IPC instead of trusting its own renderer code?**
Because the renderer binary being Chromium's own code is irrelevant once it executes untrusted web content: a single V8 or Blink memory-safety bug gives the attacker arbitrary code execution *inside* the renderer, after which it can send any bytes over Mojo. The threat model is "the renderer is the attacker". Hence: capabilities granted explicitly per process (`ChildProcessSecurityPolicy`), authority stamped into Mojo endpoints at bind time rather than read from messages, validation at every trust-boundary crossing, and kill-on-bad-message as the failure mode.

---

*Related: [renderer_process.md](renderer_process.md) (Blink/V8/cc side of these flows) · [blink.md](blink.md) (rendering engine internals) · [../C++/ipc_concepts.md](../C++/ipc_concepts.md) (general IPC mechanisms).*
