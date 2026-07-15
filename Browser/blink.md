# Blink Rendering Engine

**Mục lục:** What Blink is · Directory layout · Rendering pipeline (parse → DOM → style → layout → paint → commit) · Document lifecycle & scheduling · V8 bindings · Oilpan GC · Key concepts (events, shadow DOM, layout thrashing, containment) · What triggers what · Interview Q&A

**Sibling docs:** [browser_process.md](browser_process.md) · [renderer_process.md](renderer_process.md) · [network_process.md](network_process.md)

---

## 1. What Blink Is (and Is Not)

**Blink** is Chromium's rendering engine. It was forked from WebKit's WebCore in **April 2013** and lives in `third_party/blink/`. It runs inside the **renderer process** (see [renderer_process.md](renderer_process.md)), mostly on the renderer's **main thread**, and implements the web platform: it turns HTML/CSS/JS into a description of *what to draw*.

What Blink **does**:

| Responsibility | Details |
| --- | --- |
| **Parsing** | HTML, CSS, XML → trees (`HTMLDocumentParser`, CSS parser) |
| **DOM** | The `Node`/`Element` tree, events, Shadow DOM, MutationObserver |
| **Style** | Selector matching, cascade, `ComputedStyle` (StyleEngine) |
| **Layout** | Geometry via **LayoutNG** — box positions and sizes as an immutable fragment tree |
| **Paint** | Recording *display items* (draw commands) + building *paint property trees* |
| **Web APIs** | fetch, IndexedDB, WebGL, Service Workers, WebRTC glue… (`renderer/modules/`) |
| **JS integration** | V8 bindings (Web IDL → generated C++/JS glue) and the Blink scheduler that runs the event loop |

What Blink does **NOT** do — a classic interview trap:

| Not Blink's job | Who does it |
| --- | --- |
| Rasterizing pixels & putting them on screen | `cc/` (renderer-side compositing) + **Viz** display compositor and OOP raster in the **GPU process** |
| Networking (HTTP, DNS, cookies, cache) | **Network Service** (`services/network/`, out of process); Blink only *initiates* fetches via a `URLLoaderFactory` |
| JavaScript engine | **V8** (`v8/`) is a separate project *embedded by* Blink, not owned by it |
| Navigation decisions, tabs, omnibox | Browser process — navigation is browser-initiated (`NavigationRequest`), see [browser_process.md](browser_process.md) |
| Sandboxing / process management | `content/` layer + browser process |

So the accurate one-liner: *Blink produces a compositor-ready description of the page (layer list + property trees + recorded paint ops); everything after that — raster, compositing, display — happens in cc/Viz.*

**Why the 2013 fork?** Chromium's multi-process architecture diverged from Apple's (WebKit2 put its own process model *inside* WebKit; Chromium keeps the process model in `content/` *around* the engine). Forking let Chromium delete thousands of `#ifdef` platform ports, remove the WebKit API layer it never used, and evolve the engine aggressively — LayoutNG, CompositeAfterPaint and Oilpan (all below) are post-fork rewrites that would have been near-impossible to land upstream.

**Where Blink runs:** one Blink instance per renderer process, on the main thread. With Site Isolation and OOPIFs, one *page* may be assembled from several renderer processes, each running Blink for its `LocalFrame`s with placeholder `RemoteFrame`s for the rest — frame tree plumbing is covered in [browser_process.md](browser_process.md).

Quick engine landscape for context:

| Engine | Browsers | JS engine | Notes |
| --- | --- | --- | --- |
| **Blink** | Chrome, Edge, Opera, Brave, Herond… | V8 | This doc |
| **WebKit** (WebCore) | Safari, everything on iOS pre-EU-DMA | JavaScriptCore | Common ancestor (2013 fork) |
| **Gecko** | Firefox | SpiderMonkey | Rust components (Stylo, WebRender) |

---

## 2. Directory Layout

Blink enforces a strict dependency order (documented in `third_party/blink/renderer/README.md`): `platform` ← `core` ← `modules` ← `controller`, with `bindings` split alongside core/modules and `public/` as the outward-facing API.

| Directory | Contents | Examples |
| --- | --- | --- |
| `third_party/blink/renderer/core/` | The tightly-coupled heart of the web platform: DOM, CSS, layout, paint, frames, editing, HTML parser | `core/dom/`, `core/css/`, `core/layout/`, `core/paint/`, `core/frame/`, `core/html/parser/` |
| `third_party/blink/renderer/modules/` | Self-contained Web APIs factored out of core, each with explicit DEPS | `modules/webgl/`, `modules/indexeddb/`, `modules/service_worker/`, `modules/mediastream/`, `modules/crypto/` |
| `third_party/blink/renderer/platform/` | Lower-level building blocks with no knowledge of the DOM | `platform/scheduler/` (task queues), `platform/graphics/` (display items, paint chunks), `platform/heap/` (Oilpan), `platform/wtf/` (Vector, HashMap, String) |
| `third_party/blink/renderer/bindings/` | Everything that touches V8 APIs directly; the Web IDL compiler | `bindings/scripts/` (the `bind_gen` generator), `bindings/core/`, `bindings/modules/` |
| `third_party/blink/renderer/controller/` | Embedder-side infrastructure that *drives* Blink rather than implementing web features | memory pressure handling, Blink initialization |
| `third_party/blink/public/` | **The API boundary.** Everything else in Blink is an implementation detail; `content/` embeds Blink only through here | `public/web/` (e.g. `WebLocalFrame`), `public/platform/` (the `Platform` interface, `task_type.h`), `public/mojom/` (browser ↔ renderer Mojo interfaces), `public/common/` (shared browser/renderer code) |

Key point for interviews: `content/renderer/` (e.g. `RenderFrameImpl`) talks to Blink through `public/web/`; Blink calls back out through `public/platform/`; and browser ↔ Blink IPC is defined in `public/mojom/` and carried by **Mojo** — there is no legacy IPC in this path.

---

## 3. The Rendering Pipeline

This is the heart of Blink. Parsing runs in ordinary scheduler tasks as bytes stream in and marks the document dirty; the stages from style recalc through paint and commit run on the renderer **main thread** as one *document lifecycle update*, triggered by the compositor's `BeginMainFrame` (§4.2):

```text
  bytes from Network Service (via URLLoaderFactory + mojo data pipe)
     │
     ▼
┌───────────────┐   tokens    ┌──────────────┐
│ HTML Parsing   │───────────▶│   DOM tree    │  Node/Element hierarchy
│ HTMLDocument-  │            │  core/dom/    │  (+ Shadow DOM → flat tree)
│ Parser         │            └──────┬───────┘
│ + preload      │                   │
│   scanner      │                   ▼
└───────────────┘            ┌──────────────┐
                             │    Style      │  selector matching, cascade
   CSS bytes ──▶ CSSParser ─▶│  StyleEngine  │──▶ ComputedStyle per element
                             └──────┬───────┘
                                    ▼
                             ┌──────────────┐
                             │   Layout      │  LayoutObject tree ──▶ immutable
                             │  (LayoutNG)   │  PhysicalFragment tree (geometry)
                             └──────┬───────┘
                                    ▼
                             ┌──────────────┐
                             │   PrePaint    │  paint property trees (transform/
                             │               │  clip/effect/scroll) + invalidation
                             └──────┬───────┘
                                    ▼
                             ┌──────────────┐
                             │    Paint      │  display item list + paint chunks
                             └──────┬───────┘
                                    ▼
                             ┌──────────────┐
                             │  Commit to cc │  PaintArtifactCompositor →
                             │               │  cc::Layer list + property trees
                             └──────┬───────┘
                                    ▼
              cc compositor thread → tiling → raster (GPU process, OOP raster)
              → activation → Viz display compositor draws the frame
              (that half is covered in renderer_process.md)
```

### 3.1 Parsing — `third_party/blink/renderer/core/html/parser/`

- `HTMLDocumentParser` drives tokenization (`html_tokenizer.h`) and tree building (`html_tree_builder.h` → `html_construction_site.h`). HTML is never "invalid" — the spec defines exact error recovery for every malformed input, which is why the parser is a spec-mandated state machine, not a grammar, and why all engines build the same tree from broken markup.
- Parsing is **incremental and yielding**: the parser processes chunks as they stream from the network and periodically yields to the scheduler, so the page can style/layout/paint what it has (first contentful paint before the last byte arrives). `document.write` is the reason the parser must be able to stop dead: a script can inject markup at the current insertion point.
- **Script-blocking rules:** a plain `<script>` halts parsing (the script may `document.write`), and must additionally wait for pending stylesheets above it (scripts can read computed style — this is how CSS "blocks" JS which blocks parsing).

| Script | Fetch | Execute | Order |
| --- | --- | --- | --- |
| `<script>` | Blocks parser | Immediately when fetched | Document order |
| `<script defer>` | Parallel with parsing | After parsing, before `DOMContentLoaded` | Document order |
| `<script async>` | Parallel with parsing | Whenever ready (pauses parser briefly) | **Unordered** |
| `<script type="module">` | Parallel (+ dependency graph) | Like `defer` unless `async` | Document order |

- **Speculative preload scanner** (`html_preload_scanner.h`, with a background-thread variant in `background_html_scanner.h`): while the parser is blocked on a script, the scanner keeps scanning raw input ahead of the parser and kicks off fetches for `<img>`, `<script>`, `<link>` etc. through the Network Service. This is one of the biggest loading-performance wins in the engine — network fetches overlap script execution. It's *speculative* because `document.write` could invalidate what it saw; the common case wins hugely.

### 3.2 DOM tree — `third_party/blink/renderer/core/dom/`

The output of parsing is the DOM. The C++ class hierarchy (all Oilpan garbage-collected, §6, and paired lazily with JS wrappers, §5):

```text
EventTarget
 └─ Node                        core/dom/node.h
     ├─ ContainerNode
     │   ├─ Document            core/dom/document.h  (owns lifecycle, StyleEngine)
     │   ├─ Element             core/dom/element.h
     │   │    └─ HTMLElement → HTMLDivElement, HTMLInputElement, …
     │   └─ ShadowRoot
     └─ CharacterData → Text, Comment
```

Details that come up in interviews: `Node` is deliberately slim (flags + parent/sibling pointers); rarely-used data (dataset, shadow root pointer, animation data) hangs off *rare data* side structures so 100k text nodes stay cheap. Live collections (`getElementsByTagName`) are cached views invalidated by a DOM version counter, while `querySelectorAll` returns a static snapshot. `MutationObserver` callbacks are delivered at the **microtask checkpoint** (§4.4) — batched, unlike the synchronous (and removed) mutation events. Shadow DOM composes multiple trees into the **flat tree** (`flat_tree_traversal.h`) — style/layout consume the flat tree, not the DOM tree (§7.2).

### 3.3 Style — `third_party/blink/renderer/core/css/`

1. **Parse:** CSS text → `StyleSheetContents`/rules (`core/css/parser/`), indexed into RuleSets **bucketed by rightmost selector** (id, class, tag…) so an element only tests rules that could possibly match it.
2. **Recalc:** the `StyleEngine` (`core/css/style_engine.h`) tracks which elements are style-dirty; `StyleResolver` (`core/css/resolver/style_resolver.h`) matches selectors **right-to-left** (start from the subject element, walk ancestors only for candidate rules — cheap rejection), applies the cascade, and produces an immutable, ref-counted **`ComputedStyle`** per element. Cascade order, roughly: origin & importance (UA → user → author; `!important` reverses) → cascade layers → specificity → source order; then inheritance and initial values fill the gaps, and lengths like `em`/`%` are resolved to used values.
3. **Sharing & caching:** sibling elements with the same tag/class/attributes can *share* one `ComputedStyle`; the `MatchedPropertiesCache` reuses the applied-declarations result when a different element matched the identical rule set. Style is the stage that scales with `elements × rules`, so these caches matter.
4. **Invalidation, not recalculation:** when a class/attribute/state changes, Blink does not restyle the world. At stylesheet parse time it builds **invalidation sets** — "if class `.a` changes, descendants matching `.b` may be affected" — and at mutation time marks only those elements dirty. Documented in `third_party/blink/renderer/core/css/style-invalidation.md` and implemented in `core/css/invalidation/`.

### 3.4 Layout — LayoutNG, `third_party/blink/renderer/core/layout/`

LayoutNG **is** the layout engine (fully shipped; the `NG` class-name prefixes have been dropped). Two-tree design:

- **`LayoutObject` tree**: built from the flat tree + ComputedStyle ("attachment"). Not 1:1 with DOM — `display:none` elements get no LayoutObject; `::before`/`::after` and anonymous boxes get LayoutObjects with no DOM node.
- **Fragment tree**: layout is a pure(ish) function. For each box, a `LayoutAlgorithm` (block, inline, flex, grid, table…) takes `(BlockNode, ComputedStyle, ConstraintSpace)` — the constraint space carries available size, writing mode, fragmentation context — and returns a `LayoutResult` containing an **immutable `PhysicalFragment`** with physical (left/top) child offsets.

```text
  LayoutObject tree  (mutable, persistent across frames)
        │  Layout(ConstraintSpace)
        ▼
  PhysicalFragment tree (immutable output; a node fragmented across
  columns/pages produces MULTIPLE fragments — hence "fragment", not "box")
```

```cpp
// Simplified shape of a LayoutNG algorithm (see core/layout/layout_ng.md)
class BlockLayoutAlgorithm : public LayoutAlgorithm {
 public:
  // Inputs are immutable; output is a new immutable fragment.
  const LayoutResult* Layout() {
    for (LayoutInputNode child = Node().FirstChild(); child; /* … */) {
      ConstraintSpace child_space = CreateConstraintSpaceForChild(child);
      const LayoutResult* result = To<BlockNode>(child).Layout(child_space);
      container_builder_.AddResult(*result, child_offset);
    }
    return container_builder_.ToBoxFragment();  // → PhysicalBoxFragment
  }
};
```

Why immutability matters: results are **cacheable** (`LayoutResult` cached on the box, reused when the constraint space matches — most subtrees skip layout entirely on incremental updates), layout can't accidentally read dirty state outside its inputs, and paint/hit-testing read a consistent snapshot. Each display type gets its own algorithm (block flow, inline/line breaking in `core/layout/inline/`, flex, grid, table), all composing through the same node/space/fragment contract — which is how block fragmentation (multicol, printing) works uniformly across them. Design doc: `third_party/blink/renderer/core/layout/layout_ng.md`.

### 3.5 PrePaint + Paint — `third_party/blink/renderer/core/paint/`

Two lifecycle phases (see `third_party/blink/renderer/core/paint/README.md`):

- **PrePaint** — `PrePaintTreeWalk` walks the tree and (a) builds the **paint property trees** via `PaintPropertyTreeBuilder`: separate trees of *transform*, *clip*, *effect* (opacity/filter) and *scroll* nodes, decoupled from the element tree; (b) runs `PaintInvalidator` to decide what needs repainting.
- **Paint** — walks `PaintLayer`s in stacking order and records **display items** (immutable draw commands wrapping `cc::PaintRecord`s) into `PaintController` (`platform/graphics/paint/paint_controller.h`, `display_item.h`). Sequential items with identical property-tree state are grouped into **`PaintChunk`s** (`paint_chunk.h`). Unchanged display items are reused from cache. Painting draws nothing — it *records*:

```text
<div id=a style="opacity:0.5"><div id=b>hi</div></div>   produces roughly:

display items: [ a:Background ][ b:Background ][ b:Text "hi" ]
paint chunks:  [───────── chunk: effect=Opacity(0.5), transform=T0 ─────────]
property trees:  EffectTree:  root ── Opacity(0.5)      TransformTree: root…
```

**CompositeAfterPaint (CAP)** — the shipped architecture, and a favorite senior question. In the legacy world, Blink decided *before* paint which elements got their own composited layers, and painted into them. Now compositing is decided **after** paint: `PaintArtifactCompositor` (`platform/graphics/compositing/paint_artifact_compositor.h`) analyzes the paint chunks + property trees and *then* chooses layerization — which chunks merge into which `cc::Layer`s, based on compositing reasons (active transform animation, scroll containers, `will-change`, overlap…). Benefits: layerization decisions see the real painted output, layer explosion is easier to control, and paint no longer depends on compositing state. `PaintLayer` still exists but is a legacy structure for stacking/hit-testing order that the team wants to shrink.

### 3.6 Commit to cc

At the end of the lifecycle update, Blink hands cc a **layer list and property trees** (not a layer *tree* — CAP produces a flat list ordered by paint order, positioned by property-tree nodes). The cc **commit** copies this to the compositor thread's *pending tree* while the main thread briefly blocks; after commit the main thread is free to run JS for the next frame while the compositor thread rasters and activates the last one — the pipeline stages of consecutive frames overlap. From there on it's cc/Viz territory: tiling → raster tasks executed in the **GPU process** (OOP raster) → activation to the *active tree* → `CompositorFrame` submitted to the **Viz** display compositor, which aggregates frames from all renderers (and the browser UI) and draws. That half of the pipeline, plus why scrolling stays smooth while the main thread is busy, is covered in [renderer_process.md](renderer_process.md).

---

## 4. Document Lifecycle & Scheduling

### 4.1 DocumentLifecycle

Blink tracks pipeline progress explicitly in `DocumentLifecycle` (`third_party/blink/renderer/core/dom/document_lifecycle.h`). Real state names (abbreviated list):

```text
kVisualUpdatePending
  → kInStyleRecalc            → kStyleClean
  → kInPerformLayout          → kLayoutClean
  → kInCompositingInputsUpdate→ kCompositingInputsClean
  → kInPrePaint               → kPrePaintClean
  → kInPaint                  → kPaintClean
```

`LocalFrameView::UpdateLifecyclePhases()` (`core/frame/local_frame_view.h`) advances a frame to a target state; `UpdateAllLifecyclePhases()` runs to `kPaintClean`. DCHECKs enforce ordering — e.g. you may not read layout results while the document is not at least `kLayoutClean`. JS APIs like `offsetWidth` call the equivalent of `UpdateLifecycleToLayoutClean()` synchronously — the root cause of layout thrashing (§7.3). The lifecycle is per-`LocalFrameView` but driven for the whole local frame tree at once, so same-process iframes update together in document order.

### 4.2 Frame production: BeginMainFrame

Blink does not decide when to render. The cc scheduler (compositor thread), driven by Viz's vsync-aligned BeginFrame signal, posts **`BeginMainFrame`** to the Blink main thread when a visual update is pending:

```text
Viz (GPU proc)      compositor thread             Blink main thread
 BeginFrame ───────▶ cc::Scheduler ── needs ────▶ BeginMainFrame task:
 (vsync-aligned)         │          main frame?     1. dispatch rAF-aligned input
                         │                          2. run requestAnimationFrame cbs
                         │                          3. lifecycle: style→layout→
                         │                             prepaint→paint (§3)
                         ◀───────── commit ──────── 4. commit layer list+prop trees
                     tiling/raster/activate
```

`requestAnimationFrame` callbacks run *before* style/layout in the same frame, so a rAF write is batched into this frame's single lifecycle update — that's the whole point of rAF over `setTimeout(16)`. If nothing invalidated the document, cc never requests a main frame — an idle page consumes ~zero main-thread rendering time. And because the compositor thread can scroll and animate composited properties using its own copy of the property trees, a busy main thread (long task) janks rAF animations but not composited scrolling.

### 4.3 The Blink scheduler — `third_party/blink/renderer/platform/scheduler/`

Blink owns the renderer main-thread event loop via `MainThreadSchedulerImpl`, with per-page (`PageScheduler`) and per-frame (`FrameScheduler`) levels. Every posted task carries a **`TaskType`** (`third_party/blink/public/platform/task_type.h`) — real values include `kDOMManipulation`, `kUserInteraction`, `kNetworking`, `kNetworkingUnfreezable`, `kPostedMessage`, `kJavascriptTimerDelayedHighNesting`, `kIdleTask`. Task type determines the queue, and the queue determines policy:

| Policy | Example |
| --- | --- |
| **Prioritization** | Input and compositor tasks beat loading tasks; idle tasks (`requestIdleCallback`) run only in gaps |
| **Throttling** | JS timers in background tabs run at most ~1/s (and are budget-throttled further); timers with nesting level ≥ 5 are clamped to ≥ 4 ms (`kJavascriptTimerDelayedHighNesting`) |
| **Freezing** | Frozen/back-forward-cached pages stop most queues entirely; `kNetworkingUnfreezable` marks tasks that must survive |
| **Pausing** | `alert()`/sync XHR pause pausable queues while nested event loops run |

### 4.4 Tasks vs microtasks

- A **task** (macrotask) = one item from a scheduler queue: a timer, an event, a message.
- **Microtasks** (promise reactions, `queueMicrotask`, MutationObserver) run at a **microtask checkpoint**: when the JS stack empties — after each task, and also after each callback into JS (e.g. between two event listeners). Implemented on V8's microtask queue (`v8::MicrotasksScope`); Blink integrates it per-context.
- Consequence worth quoting in an interview: an infinite promise chain starves rendering just like a `while(true)` — rendering only happens *between tasks*, and microtasks run to completion first.

---

## 5. V8 Bindings

V8 knows nothing about the DOM; Blink teaches it via generated bindings.

- **Web IDL → C++ glue:** every web interface is defined in a `.idl` file next to its implementation (e.g. `core/dom/events/event.idl`). The IDL compiler (`third_party/blink/renderer/bindings/scripts/`, generator package `bind_gen/`, entry point `generate_bindings.py`) emits `v8_*.cc/h` files into the build's gen directory — V8 function templates, argument conversion, type checking, exception throwing. Design docs live beside it: `bindings/core/v8/V8BindingDesign.md`, `bindings/IDLExtendedAttributes.md`.

```idl
// Simplified IDL (what an engineer writes)
interface Node {
    readonly attribute Node? parentNode;
    [RaisesException] Node appendChild(Node node);
};
```

```cpp
// Simplified idea of the generated code (v8_node.cc)
void ParentNodeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Node* impl = V8Node::ToWrappableUnsafe(info.This());   // unwrap C++ object
  Node* result = impl->parentNode();
  V8SetReturnValue(info, ToV8(result, info.GetIsolate())); // wrap back to JS
}
```

- **Wrappers:** each C++ DOM object (a `ScriptWrappable`, `platform/bindings/script_wrappable.h`) gets at most one JS **wrapper object** per world, created lazily on first JS access. The wrapper holds a pointer to the C++ object in an internal field; `WrapperTypeInfo` (`platform/bindings/wrapper_type_info.h`) provides the runtime type identity for safe unwrapping. Lifetime is bidirectional: the wrapper keeps the C++ object alive (a traced V8→cppgc edge in the unified heap, §6), and expando properties JS sets on the wrapper survive as long as the DOM node does — even across "the wrapper is the only reference" periods.
- **`ScriptState`** (`platform/bindings/script_state.h`): bundles a `v8::Context` with its world and per-context data — "which JS world am I executing in right now". Async work stores a `ScriptState` to resolve promises in the right context later.
- **Isolated worlds** (`platform/bindings/dom_wrapper_world.h`): extension content scripts run in a separate `DOMWrapperWorld` — same C++ DOM, *different* wrapper objects and prototypes per world. A content script sees the page's DOM but not its JS state (no shared expandos, no monkey-patched prototypes) — a security and robustness boundary.
- **`ActiveScriptWrappable`** (`platform/bindings/active_script_wrappable_base.h`): normally an object with no JS or C++ references is collectable. But an `XMLHttpRequest` mid-flight with an `onload` listener must survive even if no variable references it. Classes opt in and answer `HasPendingActivity()`; the GC keeps their wrappers alive while true.

End-to-end, one JS call `node.appendChild(child)` costs: V8 property lookup on the wrapper prototype → generated binding callback → argument count/type checks and unwrap of both wrappers (per Web IDL coercion rules) → `Node::appendChild` C++ implementation → wrap/convert the return value → possible exception → JS. This boundary is not free, which is why hot APIs get fast paths and why "do less DOM chatter, batch into fewer calls" is real performance advice.

---

## 6. Oilpan — Garbage Collection for C++

Blink's DOM objects are garbage-collected C++, via **Oilpan** (`third_party/blink/renderer/platform/heap/`, API reference `BlinkGCAPIReference.md`). Since the "unified heap" project, Oilpan is implemented on **cppgc**, the C++ GC library that ships inside V8 (`v8/include/cppgc/`).

**Why a GC for C++?** The DOM is a dense graph of cycles: `Document` ↔ `Element` ↔ event listeners ↔ JS closures ↔ back to DOM nodes. With `scoped_refptr` reference counting, DOM↔JS cycles leak (JS wrapper keeps C++ node alive, node's listener keeps JS closure alive) — historically patched with fragile manual weakness. A tracing GC collects cycles by construction, and the **unified heap** lets V8's GC and Oilpan trace *through* each other: a JS→wrapper→C++→`Member`→…→JS cycle is discovered as one graph and reclaimed atomically.

```cpp
// Simplified Oilpan usage
class MyController : public GarbageCollected<MyController> {
 public:
  void Trace(Visitor* visitor) const {      // REQUIRED: enumerate heap refs
    visitor->Trace(element_);
    visitor->Trace(weak_observer_);
  }
 private:
  Member<Element> element_;                 // strong on-heap reference
  WeakMember<Observer> weak_observer_;      // cleared to null on collection
};

// From a non-GC object (e.g. a //content-side class):
Persistent<MyController> controller_;       // GC root, keeps object alive
```

| Handle | Where it lives | Semantics |
| --- | --- | --- |
| `Member<T>` | Inside a `GarbageCollected` class | Strong edge; must be listed in `Trace()` |
| `WeakMember<T>` | Inside a `GarbageCollected` class | Doesn't keep alive; nulled when the target dies |
| `Persistent<T>` | Off-heap (stack, non-GC objects) | A GC root; strong |
| `WeakPersistent<T>` / `CrossThreadPersistent<T>` | Off-heap | Weak / cross-thread variants |

Rules that interviews probe:

- **Raw pointers** to Oilpan objects are only safe on the stack — the GC scans the stack conservatively, so locals pin objects; storing a raw pointer in a heap object is a use-after-free waiting to happen (use `Member<>`).
- **Destructors are unreliable** for cross-object cleanup: sweep order is unspecified, so a destructor must not touch other on-heap objects (they may already be swept). Classes needing ordered cleanup use **pre-finalizers** (`USING_PRE_FINALIZER`), which run before sweeping while the graph is still intact — at a cost, so they're discouraged.
- `STACK_ALLOCATED()` / `DISALLOW_NEW()` annotations restrict how a type may be allocated; heap collections are `HeapVector<Member<T>>`, `HeapHashMap<...>` (traced), never `std::vector<Member<T>>`.
- **Cost model:** marking is incremental/concurrent and sweeping is lazy/concurrent, but safepoint pauses still happen on the main thread — GC time competes with the rendering pipeline in §3, which is why Blink cares about allocation rates in hot paths (and why `wtf/` containers exist).

---

## 7. Key Concepts

### 7.1 Event dispatch

`EventDispatcher` (`core/dom/events/event_dispatcher.h`) implements the DOM spec's three phases along a precomputed `EventPath` (`event_path.h`):

```text
window → document → … → parent   (1. CAPTURE, listeners with capture:true)
                     → target    (2. TARGET)
parent → … → document → window   (3. BUBBLE, if event.bubbles)
```

`stopPropagation()` halts traversal; `preventDefault()` only cancels the *default action* (which runs after dispatch — e.g. link navigation, checkbox toggle). Crossing shadow boundaries, the event path is **retargeted**: listeners outside a shadow root see `event.target` as the shadow *host*, preserving encapsulation.

A pipeline connection worth knowing: because a `touchstart`/`wheel` listener *might* call `preventDefault()`, the compositor thread would have to wait for the main thread before scrolling — unless the listener is registered `{passive: true}`, which promises it won't cancel, letting compositor-thread scrolling proceed immediately. This is why passive listeners are a scroll-performance feature, not just an API nicety.

### 7.2 Shadow DOM & the flat tree

A `ShadowRoot` attaches a hidden subtree to a host element; `<slot>` elements pull the host's light-DOM children into it. Style/layout/paint do not walk the DOM tree — they walk the **flat tree** (`core/dom/flat_tree_traversal.h`), the composed result after slot assignment. Consequences: CSS selectors don't cross shadow boundaries (style encapsulation), a slotted node's *flat-tree* parent is the slot while its *DOM* parent is still the host, and `LayoutObject` parentage follows the flat tree.

### 7.3 Invalidation vs forced synchronous layout (layout thrashing)

Normally Blink is lazy: mutations only mark state dirty; the actual work is batched into the next BeginMainFrame. But layout-reading APIs (`offsetWidth`, `getBoundingClientRect()`, `getComputedStyle()` for layout-dependent properties, `scrollTop`) must return fresh values, so they **force a synchronous** style+layout update if anything is dirty:

```js
// BAD: N forced layouts — write, read, write, read…
for (const el of items) {
  el.style.width = "50%";          // marks layout dirty
  console.log(el.offsetWidth);      // forces sync layout NOW
}
// GOOD: batch reads and writes — 1 layout
for (const el of items) el.style.width = "50%";
requestAnimationFrame(() => { for (const el of items) read(el); });
```

This is "layout thrashing". The senior-level explanation: it's not that reading `offsetWidth` is expensive — it's that reading it *while the document is layout-dirty* forces `UpdateLifecycleToLayoutClean()` mid-task, repeatedly.

### 7.4 Containment & `content-visibility`

- `contain: layout paint size style` lets authors promise Blink that a subtree is self-contained, so invalidations inside it don't escape and vice versa — invalidation sets and layout can stop at the boundary.
- `content-visibility: auto` goes further: offscreen subtrees **skip style recalc, layout and paint entirely** (their lifecycle work is locked), turning a 10,000-row page into "only render the viewport". `contain-intrinsic-size` supplies placeholder geometry so the scrollbar stays sane:

```css
.feed-item {
  content-visibility: auto;              /* skip rendering while offscreen */
  contain-intrinsic-size: auto 320px;    /* placeholder height for scrollbar */
}
```

These are the CSS-level levers an engineer should reach for before micro-optimizing the pipeline.

---

## 8. What Triggers What

Which pipeline stages re-run for a given mutation (stages are cumulative to the right — anything requiring layout also requires prepaint/paint checks and commit):

| Mutation | Style | Layout | Paint | Commit/cc | Notes |
| --- | :-: | :-: | :-: | :-: | --- |
| `transform` / `opacity` change (already composited, e.g. animation or `will-change`) | ✻ | — | — | ✔ | Only a paint **property node** update; can even run compositor-side only for animations — the fast path |
| `color`, `background-color`, `outline` | ✔ | — | ✔ | ✔ | Style recalc + paint invalidation ("paint-only" properties); no geometry change |
| `width`, `font-size`, `display` | ✔ | ✔ | ✔ | ✔ | Geometry changes → LayoutNG re-runs (with fragment caching limiting the blast radius) |
| DOM node insertion/removal | ✔ | ✔ | ✔ | ✔ | Invalidation sets scope the style recalc; layout tree is rebuilt locally |
| `class` change matched by descendant selectors | ✔ (scoped) | maybe | maybe | ✔ | Invalidation sets decide *which* descendants; what re-runs next depends on which properties changed |
| Scrolling a composited scroller | — | — | — | — (renderer main) | Handled on the compositor thread via the scroll property tree; main thread only gets async `scroll` events |
| `visibility: hidden` | ✔ | — | ✔ | ✔ | Box keeps its geometry — paint skips it (contrast `display:none`: full layout) |
| Inserting/enabling a stylesheet | ✔ (wide) | likely | likely | ✔ | RuleSets rebuilt; potentially broad recalc — why late-injected CSS is expensive |
| Text input into `<input>` | ✔ | ✔ (scoped) | ✔ | ✔ | Contained by the input's subtree; editing also updates selection/caret painting |
| Reading `offsetWidth` while dirty | forced | forced | — | — | Synchronous flush to `kLayoutClean` inside the current task (§7.3) |

✻ = style recalc happens but resolves to a compositor-updatable change. The design goal visible in this table: push changes as far *right* as possible — the cheapest properties to animate are the ones cc can own (`transform`, `opacity`, filter), which is exactly why CSS animation guidance says "animate transform, not top/left".

Primary in-tree documentation (all verified at `main`, excellent pre-interview reading):

| Doc | Topic |
| --- | --- |
| `third_party/blink/renderer/README.md` | Directory layout & dependency rules |
| `third_party/blink/renderer/core/layout/layout_ng.md` | LayoutNG design |
| `third_party/blink/renderer/core/paint/README.md` | PrePaint, paint, property trees, layerization |
| `third_party/blink/renderer/core/css/style-invalidation.md` | Invalidation sets |
| `third_party/blink/renderer/platform/heap/BlinkGCAPIReference.md` | Oilpan API |
| `third_party/blink/renderer/bindings/core/v8/V8BindingDesign.md` | Bindings architecture |

---

## Interview Q&A

**Q1. Walk me through what happens from HTML bytes to pixels, and where Blink's responsibility ends.**
A: Network Service streams bytes to the renderer over a Mojo data pipe → `HTMLDocumentParser` tokenizes and builds the DOM (preload scanner fetching ahead) → StyleEngine computes `ComputedStyle` via selector matching and the cascade → LayoutNG builds the LayoutObject tree and produces an immutable fragment tree with geometry → PrePaint builds paint property trees → Paint records display items/paint chunks → `PaintArtifactCompositor` layerizes and commits a cc layer list + property trees to the compositor thread. Blink's job ends at commit. cc tiles and rasters (OOP raster in the GPU process), and the Viz display compositor aggregates and draws. Blink = "describe what to draw"; cc/Viz = "actually draw it".

**Q2. What is CompositeAfterPaint and why was it a big deal?**
A: Historically Blink decided compositing (which elements get their own layers) *before* paint, then painted into those layers — so paint depended on compositing state, and layerization was decided from heuristics over the layout tree. CAP inverts this: paint first records display items grouped into paint chunks tagged with paint-property-tree state; then `PaintArtifactCompositor` decides layerization from the actual painted output. Cleaner phase separation, fewer bugs from stale compositing decisions, better control of layer explosion, and it made unified property trees the single source of truth.

**Q3. Why is LayoutNG's fragment tree immutable, and why "fragments" instead of "boxes"?**
A: Layout is modeled as a function `(node, style, ConstraintSpace) → LayoutResult/PhysicalFragment` with no access to outside state. Immutability enables safe caching (skip layout if the constraint space matches), guarantees paint/hit-testing read consistent snapshots, and eliminates a whole class of "layout read dirty state" bugs. "Fragment" because one box can produce several outputs — split across columns, pages, or lines (block fragmentation) — so the geometry tree is a tree of fragments, not a 1:1 box tree.

**Q4. How does Blink avoid recomputing style for the whole document when a class changes?**
A: Descendant invalidation sets, built at stylesheet parse time. For each class/attribute/pseudo-state appearing in selectors, Blink precomputes which descendant features could be affected when it changes. On mutation, it schedules invalidation only for elements matching those sets rather than restyling the subtree. Combined with `contain: style` and `content-visibility`, style recalc cost tracks the change, not the document size. (`core/css/invalidation/`, `core/css/style-invalidation.md`.)

**Q5. What is layout thrashing mechanically — what does the engine actually do?**
A: Mutations mark the document style/layout-dirty and normally get batched into the next BeginMainFrame. But geometry getters like `offsetWidth` must be spec-correct, so if the document is dirty they synchronously run `LocalFrameView::UpdateLifecyclePhases` up to `kLayoutClean` *inside the current JS task*. An interleaved write-read loop therefore runs full style+layout N times per frame instead of once. Fix: batch writes, then batch reads (or read before writing, use rAF, or `ResizeObserver`/`IntersectionObserver` which deliver post-layout).

**Q6. Why does Blink use a garbage collector for C++ DOM objects?**
A: The DOM is cyclic, and worse, cycles cross the C++/JS boundary: a JS wrapper keeps a C++ node alive; the node's event listener holds a JS closure that closes over the wrapper. Reference counting leaks such cycles. Oilpan is a tracing GC (`GarbageCollected<T>`, `Member<>`, `Persistent<>` roots, explicit `Trace()` methods), now built on V8's cppgc, and the **unified heap** traces the JS heap and the C++ heap as one graph — cross-language cycles are collected correctly and atomically.

**Q7. Difference between `Member<T>`, `WeakMember<T>` and `Persistent<T>`?**
A: `Member<T>` is a strong traced edge between two on-heap (GC-managed) objects and must be visited in `Trace()`. `WeakMember<T>` doesn't keep its target alive and is nulled when the target is collected. `Persistent<T>` is for *off-heap* code (stack objects, non-GC classes like //content-side objects) and acts as a GC root. Overusing `Persistent` effectively reintroduces leaks (roots never die), so it's a code-review smell.

**Q8. How do extension content scripts see the page DOM without the page seeing them?**
A: Isolated worlds. Each `DOMWrapperWorld` gets its own V8 context and its own wrapper objects for the *same* underlying C++ DOM nodes. The content script and the page each have independent prototypes and expando properties, so neither can observe or tamper with the other's JS — but both operate on the same document. `ScriptState` tells binding code which world/context is currently executing.

**Q9. When exactly do microtasks run relative to tasks and rendering?**
A: Microtasks run at microtask checkpoints — whenever the JS stack unwinds to empty: at the end of each task, and after each individual callback invocation (e.g. between two event listeners for one event). Rendering (BeginMainFrame lifecycle) happens *between tasks*, never mid-task. So a promise chain that keeps scheduling microtasks blocks rendering exactly like a synchronous loop, whereas `setTimeout(0)` yields to both rendering and input because it schedules a new task through the Blink scheduler.

**Q10. Why is animating `transform` cheap but animating `width` expensive?**
A: `transform` lives in a paint property tree node; if the element is composited, updating it is a property-tree update that cc can apply on the compositor thread — no style recalc beyond the resolved value, no layout, no repaint, and it keeps running even if the main thread is janked. `width` changes geometry: LayoutNG must re-run for the element and everything its size affects, then prepaint, repaint and commit. That's the entire main-thread pipeline versus a compositor-side matrix update.

**Q11. What role does the Blink scheduler play beyond a task queue?**
A: It's a policy engine. Every task carries a `TaskType` (`public/platform/task_type.h`) mapping to a queue with attributes: prioritizable (input > loading), throttlable (background-tab timers), freezable (back/forward cache — only `kNetworkingUnfreezable` etc. survive), pausable (`alert()`). Per-frame (`FrameScheduler`) and per-page (`PageScheduler`) scoping lets Chromium throttle an invisible cross-origin iframe without touching the main frame. This is a big part of why suspended tabs cost ~nothing.

**Q12. How does an event dispatched inside a shadow root appear to the outside page?**
A: The `EventPath` is computed up front over the flat tree, and events are *retargeted* at shadow boundaries: for listeners outside the shadow root, `event.target` is adjusted to the shadow host, hiding internal structure. `composed: false` events don't leave the shadow root at all. Capture runs from `window` down the retargeted path, then target, then bubble back up — implemented in `core/dom/events/event_dispatcher.cc`.

---

*Related: [browser_process.md](browser_process.md) — navigation, process model, Site Isolation · [renderer_process.md](renderer_process.md) — process anatomy, cc compositor, raster/Viz half of the pipeline.*


nn20022004