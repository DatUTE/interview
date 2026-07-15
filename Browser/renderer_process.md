# Chromium Renderer Process

**Mục lục:** role & threat model · process anatomy & threads · frame model (RenderFrameImpl, OOPIF) · sandbox per platform · Mojo IPC in practice · rendering data flow (cc → Viz) · V8 in the renderer · security posture · lifecycle · source map · Interview Q&A

**Siblings:** [browser_process.md](browser_process.md) · [blink.md](blink.md) · [network_process.md](network_process.md)

---

## 1. Role: The Untrusted Web Engine

The **renderer process** is where the web actually runs: it hosts **Blink** (HTML/CSS/DOM engine) and **V8** (JavaScript engine), parses untrusted bytes from the network, executes untrusted script, and produces pixels — indirectly, via the compositor pipeline.

Three properties define it:

| Property | Meaning |
| --- | --- |
| **Sandboxed** | No direct filesystem, network, or display access. Every privileged action is brokered over Mojo IPC to the browser, network, or GPU process. |
| **Untrusted** | The renderer executes arbitrary web content. The design assumption is that it **can be fully compromised** by a malicious page (a Blink/V8 exploit is "arbitrary code execution inside the sandbox"). The browser process validates everything the renderer sends. |
| **Site-scoped** | With **Site Isolation** (default on desktop), each renderer is locked to a single **site** (scheme + eTLD+1). One renderer per `SiteInstance` — or per `SiteInstanceGroup`, since multiple SiteInstances may share a process where full isolation isn't required (e.g. Android's partial isolation). |

On the browser side, each live renderer is represented by a `content::RenderProcessHost` (`content/browser/renderer_host/render_process_host_impl.cc`), which carries a **ProcessLock** restricting which sites may load into it and which stored data it may access. `docs/process_model_and_site_isolation.md` in the tree is the canonical reference.

> **Interview framing:** "The renderer is the part of the browser we expect to lose. Everything else in the architecture — sandbox, Site Isolation, IPC validation, ORB — is about limiting the blast radius when it does."

The browser-process side of every relationship in this document (RenderProcessHost, RenderFrameHost, NavigationRequest, input routing) is covered in [browser_process.md](browser_process.md); Blink's internals (DOM, style, LayoutNG, paint) are in [blink.md](blink.md).

---

## 2. Process Anatomy & Threads

### Startup

All Chromium child processes share one binary; the browser launches it with `--type=renderer` and a `--renderer-sub-type`-agnostic bootstrap:

1. `content/app` dispatches on `--type` → `RendererMain()` in `content/renderer/renderer_main.cc`.
2. `content::ChildProcess` (`content/child/child_process.cc`) starts the **IO thread** and connects the Mojo invitation inherited from the browser.
3. `RenderProcessImpl` (`content/renderer/render_process_impl.cc`) configures V8 flags; **`RenderThreadImpl`** (`content/renderer/render_thread_impl.cc`) — a subclass of `ChildThreadImpl` (`content/child/child_thread_impl.cc`) — is created **on the main thread** and initializes Blink via `RendererBlinkPlatformImpl`.
4. On Linux the process is not spawned by `exec()` at all — it is **forked from the zygote** (§4).

`RenderThreadImpl` is a per-process singleton: it owns the connection to the browser, the compositor thread, the GPU channel, and the set of `AgentSchedulingGroup`s (`content/renderer/agent_scheduling_group.cc`) that group frames for scheduling/IPC ordering purposes.

```cpp
// Simplified shape of renderer startup (content/renderer/renderer_main.cc):
int RendererMain(MainFunctionParams params) {
  base::PlatformThread::SetName("CrRendererMain");
  // Scheduler first: every main-thread task from here on goes through it.
  auto main_thread_scheduler =
      blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler(...);

  ChildProcess render_process;               // starts the IO thread
  RenderProcessImpl::Create();               // V8 flags, process-wide config
  RenderThreadImpl::Create(...);             // Mojo to browser, init Blink

  run_loop.Run();                            // main thread event loop, forever
}
```

Two structural details worth naming in an interview:

- **`AgentSchedulingGroup`**: frames that can synchronously script each other (same agent cluster) are grouped; each group is a Mojo/scheduling boundary, which is groundwork for ordering guarantees and (potential) per-group main threads.
- **The blink main-thread scheduler is not a FIFO loop.** Work is enqueued into typed **task queues** (per-frame task types: loading, JS timers, input, posted messages, internal work, …). The scheduler prioritizes across queues (input > painting-critical > loading > timers), and this is the hook that makes per-frame **throttling, pausing, and freezing** (§9) possible — you can freeze a page by stopping its queues without touching other frames in the same process.

### Thread map

```text
                    RENDERER PROCESS
 ┌───────────────────────────────────────────────────────────┐
 │ Main thread ("renderer main")                             │
 │   Blink: parsing, DOM, style, LayoutNG, paint             │
 │   V8 main isolate: JS execution, microtasks               │
 │   blink scheduler: per-frame task queues & task types     │
 ├───────────────────────────────────────────────────────────┤
 │ Compositor thread                                         │
 │   cc: LayerTreeHostImpl, tiling, scheduling               │
 │   InputHandlerProxy: scroll/pinch WITHOUT main thread     │
 ├───────────────────────────────────────────────────────────┤
 │ IO thread (ChildProcess)                                  │
 │   Mojo message pipes: receive & route/dispatch            │
 ├───────────────────────────────────────────────────────────┤
 │ Worker threads (1 per Web Worker / worklet)               │
 │   each with its OWN v8::Isolate + event loop              │
 ├───────────────────────────────────────────────────────────┤
 │ Media thread · WebAudio rendering thread                  │
 ├───────────────────────────────────────────────────────────┤
 │ ThreadPool workers: raster task serialization, V8         │
 │   background compile, Oilpan/cppgc concurrent GC          │
 └───────────────────────────────────────────────────────────┘
```

| Thread | Owner | Responsibilities |
| --- | --- | --- |
| **Main** | `RenderThreadImpl` + Blink | HTML parsing, DOM, style recalc, layout (LayoutNG), paint (generating display lists), JS on the main `v8::Isolate`, event dispatch. The blink scheduler (`third_party/blink/renderer/platform/scheduler/`) multiplexes dozens of **task queues** (task types: loading, timers, input, etc.) so it can prioritize/throttle/pause classes of work per frame. |
| **Compositor** | `RenderThreadImpl` | Runs cc's impl side (`cc::LayerTreeHostImpl`). Receives the committed layer tree, decides tiling/raster priorities, produces `CompositorFrame`s, and handles **scroll/pinch input without touching the main thread**. |
| **IO** | `ChildProcess` | Where Mojo message pipes are serviced. Messages are read here and forwarded to the bound thread's task runner (usually a main-thread blink scheduler queue); latency-critical interfaces (e.g. input) can bind directly on IO or compositor threads to skip a busy main thread. |
| **Workers** | Blink | Each `Worker`/`SharedWorker`/service worker thread gets its **own thread + own `v8::Isolate`** — no shared JS heap with the main thread; communication is `postMessage` (structured clone). |
| **Media / WebAudio** | Blink/media | Demuxing and pipeline control on the media thread; the real-time WebAudio rendering thread runs audio graphs at audio-callback priority. |
| **ThreadPool / GC** | base + V8 + Oilpan | Raster tasks (paint-op serialization toward the GPU process), V8 streaming compilation, concurrent marking/sweeping for both the JS heap and Blink's Oilpan (cppgc) C++ heap. |

**Key point for interviews:** the renderer is *not* single-threaded, but the DOM is single-threaded by spec — so the architecture's whole game is moving work **off** the main thread (compositor scrolling, raster workers, streaming compile, concurrent GC) while keeping DOM mutation on it.

---

## 3. Frame Model: RenderFrameImpl, LocalFrame, OOPIF

Modern Chromium is **frame-based**, not page-based. Since Site Isolation, a single page's frame tree can span multiple renderer processes, so the unit of IPC and navigation is the **frame** (and with **RenderDocument**, increasingly the *document within* a frame).

### The per-frame object stack

```text
 BROWSER PROCESS                      RENDERER PROCESS
 ┌──────────────────────┐   Mojo    ┌─────────────────────────┐
 │ RenderFrameHostImpl  │◄────────► │ content::RenderFrameImpl│
 │ (render_frame_host_  │           │ (render_frame_impl.cc)  │
 │  impl.cc)            │           ├─────────────────────────┤
 └──────────────────────┘           │ blink::WebLocalFrame    │  ← public API layer
                                    │  (WebLocalFrameImpl)    │
                                    ├─────────────────────────┤
                                    │ blink::LocalFrame       │  ← core Blink
                                    │  └─ Document, DOM, ...  │
                                    └─────────────────────────┘
```

- `content::RenderFrameImpl` (`content/renderer/render_frame_impl.cc`) is the content-layer object; it owns the Mojo endpoints paired with the browser's `RenderFrameHostImpl` and implements content-side concerns (navigation commit, media, plugins hooks).
- `blink::WebLocalFrame` (`third_party/blink/public/web/`) is the stable public boundary between content and Blink; `blink::WebLocalFrameImpl` wraps the core `blink::LocalFrame` (`third_party/blink/renderer/core/frame/local_frame.cc`), which owns the `Document`.
- Historically there was one `RenderView` per tab; `RenderView`/`RenderViewHost` survive only as legacy remnants (roughly "per-frame-tree state"), and **MPArch** (multiple page architecture — prerendering, back/forward cache, fenced frames as multiple FrameTrees in one WebContents) further buried the "one tab = one page" assumption.

### OOPIF: stitching one page from several processes

With out-of-process iframes, each process renders only the frames that belong to *its* site. Every other frame in the tree is represented locally by a **placeholder**:

- In Blink: `blink::RemoteFrame` / `WebRemoteFrameImpl` (`third_party/blink/renderer/core/frame/remote_frame.cc`) — no Document, no layout tree; it knows only the frame's size, its `postMessage` channel, and the **surface** to embed.
- In the browser: `content::RenderFrameProxyHost` (`content/browser/renderer_host/render_frame_proxy_host.h`) pairs with each RemoteFrame and relays cross-process frame interactions (`postMessage`, focus, window.open relationships). *(The old content-side `RenderFrameProxy` class was folded into Blink's `RemoteFrame` during the Onion Soup refactor — cite `RemoteFrame` as the current class.)*

Visual stitching does **not** happen by sharing pixels between renderers. Each renderer submits its own `CompositorFrame` to **Viz** in the GPU process, and the parent's frame embeds the child's output by referencing its **SurfaceId**. Viz aggregates all surfaces into the final display frame:

```text
  a.com renderer ──CompositorFrame(embeds SurfaceId S2)──►┐
                                                          │  Viz (GPU proc)
  b.com renderer ──CompositorFrame(SurfaceId S2)─────────►┤  surface
                                                          │  aggregation
  browser UI    ──CompositorFrame──────────────────────── ►┘      │
                                                            final frame → screen
```

So a page with a cross-site iframe is: two renderer processes, two frame trees stitched logically by `RemoteFrame`/`RenderFrameProxyHost` and visually by Viz surface embedding.

Two consequences of the OOPIF model worth knowing:

- **Local roots & widgets.** Within one renderer, a contiguous subtree of local frames shares one compositor/widget; each **local root** (a local frame whose parent is remote) gets its own widget and its own `cc::LayerTreeHost` — so one renderer process may run several independent compositing pipelines for disjoint fragments of the same page.
- **Frame swaps.** When a frame navigates cross-site, the browser picks a new process and the frame is *swapped*: in the old process the `LocalFrame` is replaced by a `RemoteFrame`; in the new process a `RemoteFrame` (or nothing) is replaced by a new `LocalFrame`. The frame's identity in the tree persists; its local/remote *representation* per process changes.

### Navigation, from the renderer's seat

Navigation is **browser-initiated** (PlzNavigate is the only model). The renderer's role is deliberately small:

```text
 renderer (old doc)          browser                        renderer (target)
 ──────────────────          ───────────────────────        ─────────────────
 link click → BeginNavigation ──► NavigationRequest:
                                  - fetch via Network Service
                                  - choose SiteInstance/process
                                  - security checks
                                  CommitNavigation ─────────► RenderFrameImpl:
                                                              create new Document,
                                                              stream body, run
                                                              unload/commit dance
                                  ◄───── DidCommitProvisionalLoad (validated!)
```

The renderer never fetches the main resource itself, never picks its own process, and its `DidCommitProvisionalLoad` message back to the browser is checked against the browser's own `NavigationRequest` state — a mismatch is treated as a compromised renderer. With **RenderDocument**, cross-document navigations create a fresh `RenderFrameHost` (and renderer-side counterpart) even same-site, so "RenderFrameHost ≈ one document" is the modern invariant.

---

## 4. The Sandbox

The rule: **a renderer can compute, but cannot do I/O.** No file open, no socket, no window handle. Anything that touches the OS goes over Mojo to a more privileged process which applies policy first (e.g. "does this process's ProcessLock permit reading this origin's cookies?").

| Platform | Mechanism | Key pieces |
| --- | --- | --- |
| **Linux / ChromeOS** | **Layer 1:** namespaces — user, PID, network namespaces via the setuid/namespace sandbox, giving an empty view of the filesystem and no network. **Layer 2:** **seccomp-BPF** syscall filter — a BPF program the kernel evaluates on *every syscall*, allowing only a whitelist (and rejecting/emulating the rest). | `sandbox/policy/linux/sandbox_seccomp_bpf_linux.cc`, per-process-type policies like `sandbox/policy/linux/bpf_renderer_policy_linux.cc`; docs in `sandbox/linux/README.md` (cross-platform summary: `docs/security/process-sandboxes-by-platform.md`) |
| **Windows** | Restricted **access token** (lockdown level — almost every privilege stripped), **job object** limits, alternate desktop, and **win32k lockdown** (`PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY`) so the renderer cannot call the enormous win32k.sys kernel attack surface at all. App Container / LPAC where available. | `sandbox/win/`, policy layer in `sandbox/policy/win/sandbox_win.cc`; design doc `docs/design/sandbox.md` |
| **macOS** | **Seatbelt** (`sandbox_init` profiles, SBPL): a kernel-enforced profile compiled from a policy file that denies filesystem/network/Mach access except what's explicitly allowed. Applied at process startup before any web content loads. | `sandbox/mac/seatbelt.cc`, `sandbox/mac/seatbelt_exec.cc` |
| **Android** | OS-level **isolatedProcess** services: the renderer runs as an isolated app process with a distinct UID, plus seccomp-BPF. | `sandbox/policy/` + Android service manifests |

### Why the zygote exists on Linux

On Linux, renderers are forked from a **zygote process** (`content/zygote/zygote_main_linux.cc`, `content/zygote/zygote_linux.cc`) rather than exec'd fresh:

1. **Fork before sandbox, initialize before lockdown.** The zygote starts at boot, loads the binary, dynamic libraries, ICU data, V8 snapshot — all things that require filesystem access — *then* enters the sandbox. Every renderer forked afterwards inherits an already-initialized image and never needs filesystem access to start. (This also sidesteps the problem that after a Chrome update, the binary on disk no longer matches the running version — the zygote still holds the old image.)
2. **Shared pages.** `fork()` gives copy-on-write sharing of the binary, .rodata, and the V8 snapshot across all renderers — significant memory savings with dozens of processes.
3. **Fast startup.** No exec, no relinking, no re-parsing of startup data.

Windows and macOS have no zygote (no `fork()` semantics worth using); they rely on cheap process creation plus the mechanisms above, with the same "initialize what you need, then lock down before loading web content" discipline.

### What gets brokered instead

| Renderer needs… | Who provides it | Over what |
| --- | --- | --- |
| Network fetch | Network Service (out-of-process, `services/network/`) | `URLLoaderFactory` minted by the browser per frame |
| File access (downloads, `<input type=file>`) | Browser process | user-gesture-gated Mojo interfaces; the browser opens the file and passes a handle/data pipe |
| Fonts | Browser/OS broker | platform font services proxied (e.g. `content/child/dwrite_font_proxy/` on Windows) |
| GPU / rasterization | GPU process | command buffer over the GPU channel; OOP raster (§6) |
| Pixels on screen | Viz in the GPU process | `CompositorFrameSink` |
| Storage (cookies via `document.cookie`, IndexedDB, cache) | Browser / Storage Service side, checked against ProcessLock | per-origin Mojo interfaces |
| Clipboard, permissions-gated devices (camera, geolocation…) | Browser process, after permission checks in trusted UI | brokered Mojo interfaces |

The generalization: **every arrow out of the renderer is a Mojo interface whose far end applies policy.** Sandboxing is only half the design; the other half is that the brokers themselves are capability-scoped (a frame's factory/interfaces are bound to *that frame's* origin and process lock).

---

## 5. Mojo IPC in Practice

Mojo is **the** IPC system. Legacy `IPC::Message`/`IPC_MESSAGE_HANDLER` macros are deprecated and almost entirely removed; new code defines a `.mojom` interface, and messages are strongly typed, validated at deserialization, and routable to any thread or process.

### Concepts

- **Message pipe:** bidirectional, ordered channel; endpoints are transferable across processes (that's how interfaces are handed around).
- **Interface:** a `.mojom` definition compiled into C++ `Remote<T>` (caller side) and `Receiver<T>` (implementation side) bindings.
- **Broker pattern:** the renderer can't create arbitrary OS resources, so it *asks*. Each frame gets a `BrowserInterfaceBroker` connection (`third_party/blink/public/mojom/browser_interface_broker.mojom`) — a single `GetInterface(GenericPendingReceiver)` method through which the renderer requests browser-side interfaces, and the browser decides per frame/origin what to bind.

### Simplified example

```mojom
// example: services/network/public/mojom/url_loader_factory.mojom (simplified)
interface URLLoaderFactory {
  CreateLoaderAndStart(pending_receiver<URLLoader> loader,
                       int32 request_id,
                       uint32 options,
                       URLRequest request,
                       pending_remote<URLLoaderClient> client);
};
```

```cpp
// Renderer side (simplified): fetch without ever touching a socket.
// The renderer never ASKS for this factory — the browser PUSHED it in the
// CommitNavigation IPC (a blink::PendingURLLoaderFactoryBundle in the commit
// params), with origin/IsolationInfo already fixed browser-side, pointing at
// the Network Service.
mojo::Remote<network::mojom::URLLoaderFactory> factory(
    std::move(pending_subresource_factory_from_commit_params));

factory->CreateLoaderAndStart(loader.BindNewPipeAndPassReceiver(),
                              /*request_id=*/1, /*options=*/0,
                              request, client_remote);
// Response bytes arrive as a mojo data pipe → fed to Blink's loader.
```

Points interviewers probe:

- **The renderer never opens sockets.** Fetching goes through a `URLLoaderFactory` whose *parameters* (origin, cookie access, CORS mode) were fixed by the **browser** when it minted the factory — a compromised renderer can't ask for someone else's cookies just by lying in the request, because trust is attached to the pipe, not the message. Note the factory is **pushed, not pulled**: it arrives in the `CommitNavigation` params, whereas `BrowserInterfaceBroker.GetInterface()` is the pull path for *other* per-frame capabilities (clipboard, permissions, device APIs) — the broker does not vend `URLLoaderFactory`.
- **Interface brokering is a security choke point:** the browser refuses interfaces a frame shouldn't have (wrong origin, wrong process type) and can `mojo::ReportBadMessage` → kill the renderer on malformed/out-of-contract messages.
- **Typed validation:** generated deserialization code rejects malformed messages before app code sees them (a big step up from hand-rolled `IPC::Message` readers).

```cpp
// Browser side (simplified): every renderer message is hostile input.
void RenderFrameHostImpl::OnSomeRendererMessage(const url::Origin& claimed) {
  if (!policy->CanAccessDataForOrigin(GetProcess()->GetID(), claimed)) {
    mojo::ReportBadMessage("origin not allowed for this process");  // → kill
    return;
  }
  // proceed with the origin the BROWSER computed, not the claimed one
}
```

### Threading & ordering

- Bindings dispatch on the task runner they were bound with; renderer-side interfaces typically bind to a **blink scheduler task queue** (so frame freezing pauses their dispatch too), while latency-sensitive ones bind on the IO or compositor thread.
- Independent message pipes have **no cross-pipe ordering guarantee**. When ordering with another interface matters (classic case: messages that must interleave correctly with navigation), **associated interfaces** share the underlying channel/pipe to preserve FIFO ordering.
- Sync calls exist (`[Sync]`) but are heavily restricted, especially renderer→browser, because a blocked renderer main thread is a jank/deadlock hazard.

### Legacy IPC

The old system — `IPC::Channel`, `IPC_MESSAGE_MACROS`, giant `OnMessageReceived` switch statements routed by `routing_id` — is deprecated; content's renderer↔browser messages have been converted to Mojo interfaces (the multi-year "Onion Soup" effort also moved much content-renderer logic into Blink itself). Mention it only as history; new code never adds legacy IPC messages.

See `mojo/README.md` and `docs/mojo_and_services.md` in the tree.

---

## 6. Rendering Data Flow Out of the Renderer

The renderer produces **no pixels on screen directly**. The pipeline (modern: CompositeAfterPaint, OOP raster, Viz):

```text
 MAIN THREAD (Blink)          COMPOSITOR THREAD (cc)         GPU PROCESS
 ────────────────────         ──────────────────────         ─────────────────
 style → LayoutNG layout
 → paint: display lists
   (paint ops) + property
   trees (CompositeAfterPaint:
   layerize AFTER paint)
        │  commit (blocking handoff)
        ▼
                              cc::LayerTreeHostImpl
                              tiling: visible tiles first
                              raster tasks ──serialize paint ops──►  OOP raster:
                              (worker threads)   via GPU channel     Skia executes
                                                                     into GPU memory
                              build CompositorFrame
                              (quads referencing tiles)
                                   │ SubmitCompositorFrame
                                   ▼ (CompositorFrameSink, Mojo)
                                                              Viz display compositor
                                                              aggregates surfaces from
                                                              ALL renderers + browser UI
                                                              → draw → swap → screen
```

- **Commit:** Blink's main-thread output (cc layer list + property trees + display lists) is copied to the compositor thread's pending tree (`cc/`, see `cc/README.md`, `cc/trees/layer_tree_host.cc`).
- **Raster is out-of-process:** raster "tasks" on renderer worker threads don't rasterize locally — they serialize paint ops over the command buffer to the **GPU process**, where Skia rasterizes into GPU-backed tiles. The renderer sandbox never needs GPU driver access.
- **Submission:** the compositor thread submits a `CompositorFrame` — quads + resource references, *not pixels* — through the `CompositorFrameSink` Mojo interface (`services/viz/public/mojom/compositing/compositor_frame_sink.mojom`) to the **Viz display compositor** in the GPU process (`components/viz/`), which aggregates every client's surface (§3) and draws.

### Frame pacing: BeginFrame

The renderer does not free-run. **Viz drives the clock**: it sends `OnBeginFrame` ticks (vsync-aligned) to clients that declared `SetNeedsBeginFrame(true)`. On each tick the renderer's compositor thread decides whether it can produce a frame purely impl-side (scroll/animation on committed content) or must first request a **main frame** (Blink runs rAF callbacks → style → layout → paint → commit). If nothing changed, it answers `DidNotProduceFrame` so Viz doesn't wait. This pull model is why a busy main thread degrades to "checkerboarding + still-smooth scrolling" instead of a frozen screen.

### Threaded animation & scrolling

Compositor-driven effects — transform/opacity animations, scroll offset, some scroll-linked effects — are mirrored into the impl-side tree and advanced by the compositor thread every BeginFrame, then synced back to Blink at the next commit. This is the mechanical reason the classic advice "animate transform/opacity, not width/left" works: the former animate impl-side with zero main-thread involvement; the latter require layout, hence a main frame.

### Input: compositor-first fast path

Input events go **browser → renderer compositor thread → (maybe) main thread**:

1. The browser's input router targets the right renderer and sends events over Mojo, where they land on the **compositor thread** first (`blink::WidgetInputHandlerManager` / `InputHandlerProxy`, `third_party/blink/renderer/platform/widget/input/`).
2. If the scroll can be handled purely by cc (no relevant JS handler, scroller composited), the compositor thread scrolls immediately — **60/120fps scrolling even with a blocked main thread**.
3. Only if there's a **non-passive** JS listener (wheel/touch) in the region must the event block on the main thread — which is exactly why `{passive: true}` listeners exist: they let the compositor scroll without waiting for JS.

---

## 7. V8 in the Renderer

- **One main isolate per renderer**: all frames on the main thread share a single `v8::Isolate` (heap + GC). Each worker thread has its own isolate (§2).
- **`v8::Context` per frame *and per world***: a context is a JS global scope. Every frame gets a **main world** context; **isolated worlds** give extension content scripts (and DevTools) a *separate* context over the **same DOM** — they see the same elements but different JS globals/prototypes, so page script can't tamper with an extension's functions. Blink models this as `blink::DOMWrapperWorld` (`third_party/blink/renderer/platform/bindings/`).
- **Site Isolation caveat:** same-*site* frames may share a process and isolate; contexts (not processes) enforce same-origin JS boundaries *within* a site — which is why Site Isolation's unit is the site, not the origin (`document.domain` legacy).

```text
 v8::Isolate (renderer main thread — ONE heap, ONE GC)
 ├── frame A ── main-world v8::Context        ← page script
 │          ├── isolated-world Context (ext1) ← content script, same DOM,
 │          └── isolated-world Context (ext2)    different globals
 └── frame B ── main-world v8::Context        ← same-site frame, same isolate

 Worker thread: its own v8::Isolate ── one Context   ← no shared heap at all
```

- **Streaming compilation:** scripts are compiled on background threads *while still downloading* (`third_party/blink/renderer/bindings/core/v8/script_streamer.cc`). Compiled bytecode is also **code-cached** (via the browser-side cache, keyed per site under Site Isolation) so hot scripts skip parsing on revisit.
- **Bindings:** DOM objects are C++ (Blink, Oilpan-managed) exposed to JS through generated bindings from Web IDL; the JS wrapper ↔ C++ object lifetime is coordinated between V8's GC and Oilpan (unified heap / cppgc). Full detail in [blink.md](blink.md).

---

## 8. Security Posture: Assume Compromise

The threat model is explicit: **a compromised renderer is a supported scenario**, not a failure of design. Consequences:

| Defense | What it buys |
| --- | --- |
| **Sandbox (§4)** | Compromised renderer can't touch disk/network/OS directly — it can only speak Mojo. |
| **Browser-side IPC validation** | The browser treats every renderer message as attacker-controlled: origin claims are checked against what the browser already knows (`RenderFrameHostImpl`'s committed origin, ProcessLock), bad messages → `ReportBadMessage` → renderer killed. Rule: *never trust renderer-provided origins, URLs, or indices.* |
| **Browser-initiated navigation** | Navigation (PlzNavigate — the only model) runs in the **browser**: `NavigationRequest` does the fetch via the Network Service, picks the target `SiteInstance`/process, and only then tells a renderer to commit. A renderer cannot navigate itself to a privileged URL or into another site's process. |
| **Site Isolation** | Even with arbitrary code execution in the renderer, the attacker's *address space* only ever contains data from one site. Cross-site documents live in other processes. |
| **Spectre motivation** | Site Isolation's urgency came from Spectre (2018): speculative-execution side channels let JS read **anything in its own process** without a bug in Chrome. The only durable answer is to keep cross-site data *out of the process entirely.* |
| **CORB → ORB** | Site Isolation is useless if a renderer can just `<img src="https://bank.com/secrets.json">` and get the bytes into its process. **Cross-Origin Read Blocking**, now generalized as **Opaque Response Blocking (ORB)** in the Network Service (`services/network/orb/orb_impl.cc`), sniffs no-cors responses and withholds cross-origin data (JSON/HTML/XML) that no legitimate no-cors consumer (img/script/media) could use — the bytes never reach the renderer. |

Also in the renderer's containment story: per-frame `URLLoaderFactory` parameters fixed by the browser (§5), CORS enforced in the Network Service (not trusting the renderer), and Mojo interface brokering filtered per frame/origin.

### The compromised-renderer scorecard

A crisp way to answer "so what *can* an attacker with renderer code execution do?":

| Capability | Compromised renderer… | Stopped by |
| --- | --- | --- |
| Read/write local files | ✗ | sandbox |
| Open raw sockets | ✗ | sandbox + Network Service model |
| Read another **site's** cookies/DOM/responses | ✗ | Site Isolation (ProcessLock) + ORB + browser-side checks |
| Read everything of **its own** site already in-process | ✓ | — (this is the accepted loss) |
| Lie in Mojo messages ("my origin is X") | attempts ✗ | browser validates against its own state; `ReportBadMessage` kills |
| Ask for arbitrary browser interfaces | ✗ | `BrowserInterfaceBroker` binds only what policy allows that frame |
| Attack the kernel | hard | seccomp-BPF / win32k lockdown / Seatbelt shrink the syscall surface — a *second* bug (sandbox escape) is required |

Hence the classic exploit-chain shape: **renderer RCE (V8/Blink bug) → sandbox escape (kernel or browser-IPC bug) → persistence.** Chromium's architecture forces attackers to pay for at least two independent bugs.

---

## 9. Lifecycle

| Phase | Mechanism |
| --- | --- |
| **Creation** | Browser decides it needs a process for a `SiteInstance` (navigation). It may take the **spare renderer** — a pre-warmed, unlocked `RenderProcessHost` kept ready so navigation doesn't pay process-startup latency — and then locks it to the site. |
| **Reuse** | Subject to a soft process limit, the browser reuses suitable existing processes (same ProcessLock, same profile/StoragePartition); OOPIF subframes reuse aggressively to bound memory. Logic in `content/browser/renderer_host/render_process_host_impl.cc`. |
| **RenderDocument** | Historically same-site navigations reused the `RenderFrameHost`/document plumbing; **RenderDocument** gives every *document* a fresh `RenderFrameHost` even for same-site navigations — simplifying lifetime reasoning (a RFH ≈ one document). |
| **Backgrounding** | The blink scheduler throttles background tabs (timer alignment/budget), then **freezes** eligible pages (task queues stopped entirely); background priority is dropped at the OS level. Back/forward cache (MPArch) keeps recently navigated-from documents frozen-but-alive in the renderer for instant restore. |
| **Memory pressure** | Browser-coordinated purge + V8/Oilpan GC under pressure signals; on extreme pressure/OOM the OS or the browser discards background tabs (their renderers are killed; the tab reloads on next focus). |
| **Crash** | Renderer crash ≠ browser crash: the `RenderProcessHost` observes process exit, the tab shows the **sad tab** page, other tabs/processes are untouched, and reload spawns a fresh renderer. This fault isolation is the original (pre-security) argument for the multi-process model. |
| **Priorities** | The browser adjusts renderer OS priority by visibility (foreground/background; audio playback keeps a backgrounded renderer at higher priority), which also feeds the blink scheduler's throttling decisions. |
| **Shutdown** | When the last frame in a process goes away (with keep-alive grace for `unload`/fetch keepalive), the browser asks it to exit; fast shutdown skips renderer-side teardown when safe. |

A useful mental model for the whole lifecycle: **the browser process is the renderer's kernel.** It decides when a renderer is born (navigation → SiteInstance → spare or new process), what it may touch (ProcessLock + brokered Mojo capabilities), how much CPU attention it gets (priority + scheduler policy), and when it dies (last frame gone, discard, or `ReportBadMessage`). The renderer itself holds almost no authority over its own existence — by design.

---

## 10. Source Map — Quick Reference

| Concept | Where to look in the tree |
| --- | --- |
| Renderer main-thread singleton | `content/renderer/render_thread_impl.cc` (`RenderThreadImpl`) |
| Renderer entry point / startup | `content/renderer/renderer_main.cc`, `content/child/child_process.cc`, `content/child/child_thread_impl.cc` |
| Per-frame renderer object | `content/renderer/render_frame_impl.cc` (`RenderFrameImpl`) |
| Browser-side peers | `content/browser/renderer_host/render_frame_host_impl.*`, `render_process_host_impl.cc`, `render_frame_proxy_host.h` |
| Blink frame core / OOPIF placeholders | `third_party/blink/renderer/core/frame/local_frame.cc`, `remote_frame.cc`, `web_local_frame_impl.cc`, `web_remote_frame_impl.cc` |
| Interface brokering | `third_party/blink/public/mojom/browser_interface_broker.mojom` |
| Fetching | `services/network/public/mojom/url_loader_factory.mojom`, `services/network/README.md` |
| Linux sandbox | `sandbox/policy/linux/` (seccomp-BPF policies), `content/zygote/zygote_main_linux.cc`, `sandbox/linux/README.md` |
| Windows / macOS sandbox | `sandbox/policy/win/sandbox_win.cc`, `sandbox/mac/seatbelt.cc`, `docs/design/sandbox.md` |
| Compositor (renderer side) | `cc/README.md`, `cc/trees/layer_tree_host.cc` |
| Display compositor & frame submission | `components/viz/`, `services/viz/public/mojom/compositing/compositor_frame_sink.mojom` |
| Compositor-thread input | `third_party/blink/renderer/platform/widget/input/` (`InputHandlerProxy`, `WidgetInputHandlerManager`) |
| Main-thread scheduling | `third_party/blink/renderer/platform/scheduler/` |
| ORB | `services/network/orb/orb_impl.cc` |
| Process model doc | `docs/process_model_and_site_isolation.md` |

---

## Interview Q&A

**Q1. Why does Chromium run web content in separate renderer processes at all?**
Two reasons, historical then security. (1) *Fault isolation*: a crash or hang from one page's Blink/V8 doesn't take down the browser — you get a sad tab. (2) *Security*: the renderer parses hostile input with a huge attack surface; putting it in a sandboxed process means an exploited renderer still can't touch the filesystem, network, or other sites' data. Site Isolation extended this from "web vs. browser" to "site vs. site."

**Q2. What exactly can a renderer process *not* do, and how does it get those things done?**
It cannot open files, create sockets, or talk to the display/GPU driver. Everything is brokered over Mojo: fetching via a browser-minted `URLLoaderFactory` to the Network Service, storage via browser-side interfaces gated by ProcessLock, pixels via `CompositorFrame`s submitted to Viz in the GPU process, and generic capabilities requested through `BrowserInterfaceBroker.GetInterface()`, where the browser applies per-frame/per-origin policy.

**Q3. Walk me through the threads in a renderer and why the compositor thread exists.**
Main thread (Blink + V8 + layout/paint + blink scheduler task queues), compositor thread (cc impl side + input), IO thread (Mojo dispatch), one thread per Web Worker (own isolate), media/WebAudio threads, and thread-pool workers for raster serialization, background compile, and concurrent GC. The compositor thread exists so scrolling, pinch, and animation of already-rastered content continue at frame rate even when the main thread is blocked in JS or layout — input is delivered to the compositor thread first and only hits the main thread if a non-passive handler forces it.

**Q4. A page has a cross-site iframe. Describe what exists where.**
Two renderer processes, one per site (SiteInstance). The embedder's process has `RenderFrameImpl`+`LocalFrame` for its own frame and a `blink::RemoteFrame` placeholder for the iframe (no DOM, no layout — just size + a SurfaceId); vice versa in the iframe's process. The browser holds a `RenderFrameHostImpl` for every frame plus `RenderFrameProxyHost`s representing each frame in the *other* renderers, relaying `postMessage` and layout constraints. Visually, each renderer submits its own CompositorFrame and Viz stitches them by surface embedding.

**Q5. Why is the sandbox alone not enough — why Site Isolation?**
The sandbox stops the renderer reaching the OS, but pre-isolation one process could hold multiple sites' DOMs, cookies-adjacent data, and cached responses, so a Blink/V8 bug (or Spectre, with *no* bug at all) let one site read another's data in-process. Site Isolation makes the process boundary the site boundary: a compromised or speculatively-leaky renderer only has its own site in its address space. ORB completes it by keeping cross-origin no-cors data (JSON/HTML) from ever entering the process.

**Q6. How does a fetch initiated by `fetch()` in JS actually reach the network?**
Blink's loader takes the request to the frame's `URLLoaderFactory` remote — a Mojo interface whose receiver lives in the **Network Service** and whose parameters (origin, IsolationInfo, CORS mode, cookie access) were fixed by the **browser** when the frame committed. `CreateLoaderAndStart()` streams the response back as a Mojo data pipe. The renderer never resolves DNS or opens a socket, CORS/ORB are enforced in the Network Service, and a lying renderer gains nothing because trust rides on the pipe, not on message contents.

**Q7. What does the zygote on Linux solve?**
Three problems: (a) renderers are sandboxed with no filesystem access, but startup needs to load .so's, ICU data, and the V8 snapshot — so a zygote does that once *before* sandboxing and every renderer is `fork()`ed from it already initialized; (b) fork gives copy-on-write sharing of those pages across dozens of renderers; (c) after a Chrome update replaces the binary on disk, the zygote still forks version-matched renderers from the in-memory image.

**Q8. The browser receives a Mojo message from a renderer claiming "commit this navigation to `https://bank.com`". What must it check?**
Everything — renderer messages are attacker-controlled. The browser checks the claim against its own state: navigations are browser-initiated (`NavigationRequest` in the browser did the fetch and chose the process), so a renderer-side commit must match the pending NavigationRequest; the origin must be committable under the process's ProcessLock; parameters out of contract trigger `mojo::ReportBadMessage`, which kills the process. General rule: the browser never trusts renderer-supplied origins/URLs, it derives them from what it already verified.

**Q9. How do pixels get from Blink to the screen?**
Main thread: style → LayoutNG layout → paint into display lists + property trees (CompositeAfterPaint layerizes after paint). Commit hands this to the compositor thread, where cc tiles the content and schedules raster tasks; with OOP raster those tasks serialize paint ops over the GPU channel and Skia rasterizes in the GPU process. cc then submits a `CompositorFrame` (quads + resource refs, no pixels) via `CompositorFrameSink` to the Viz display compositor in the GPU process, which aggregates surfaces from all renderers and the browser UI, draws, and swaps.

**Q10. One isolate, many contexts — explain V8's structure in the renderer.**
The main thread has one `v8::Isolate` (one JS heap/GC) shared by all frames in the process. Each frame gets a `v8::Context` per "world": the main world for page script, plus isolated worlds for extension content scripts and DevTools — same DOM, separate globals, so the page can't tamper with extension code. Workers are fully separate: own thread, own isolate. Same-origin policy within a process is enforced at the context/binding level; cross-*site* separation is enforced by process boundaries.

**Q11. What happens when a background tab "uses too much memory"?**
Escalating ladder: the blink scheduler first throttles its timer queues, then may freeze the page entirely (all task queues stopped); under memory pressure signals the renderer purges caches and triggers V8/Oilpan GC; if pressure persists the browser (or OS, e.g. Android LMK) discards the tab — kills the renderer — and the tab silently reloads when refocused. Back/forward-cached documents are the same idea deliberately: frozen in-renderer, evictable anytime.

**Q12. RenderFrameHost vs RenderFrameImpl vs LocalFrame — who owns what?**
`RenderFrameHostImpl` (browser) is the trusted authority: committed origin, lifecycle, the Mojo endpoints, policy checks. `RenderFrameImpl` (renderer, content layer) is its untrusted peer, bridging content-level concerns into Blink. `blink::WebLocalFrame`/`LocalFrame` (Blink) own the actual `Document`/DOM. With RenderDocument, a new document means a new RenderFrameHost even same-site, making "one RFH ≈ one document" the modern invariant; RenderView/RenderViewHost are legacy remnants of the page-based era.
