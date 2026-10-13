# Browser Internals — Chromium

Tài liệu ôn phỏng vấn **browser engineer** (Chromium-based). Nội dung dựa trên kiến trúc Chromium hiện đại (2025-era), mọi source path và class name đã được đối chiếu với tree thật tại [source.chromium.org](https://source.chromium.org/chromium/chromium/src).

---

## Documents

| File | Nội dung chính |
|------|----------------|
| [browser_process.md](browser_process.md) | Process model, startup (`ContentMain` → `BrowserMainLoop`), UI/IO threads, `WebContents` / `RenderProcessHost` / `RenderFrameHost`, navigation (PlzNavigate, `NavigationRequest`, RenderDocument, MPArch), site isolation & `ChildProcessSecurityPolicy`, servicification (Network Service), Mojo brokering |
| [renderer_process.md](renderer_process.md) | Threads (main / compositor / IO / workers), `RenderFrameImpl` ↔ `RenderFrameHost`, OOPIF & `RemoteFrame`, sandbox per platform (zygote + seccomp-BPF, win32k lockdown, Seatbelt), Mojo & `URLLoaderFactory`, cc → Viz data flow, OOP raster, compromised-renderer threat model |
| [blink.md](blink.md) | Rendering pipeline (parse → DOM → style → layout NG → paint → CompositeAfterPaint → commit to cc), `DocumentLifecycle` & Blink scheduler, V8 bindings (Web IDL, wrappers, isolated worlds), Oilpan GC, layout thrashing, mutation → pipeline-stage mapping |
| [network_process.md](network_process.md) | Network Service & backend communication: `SimpleURLLoader` → Mojo → `//net` → BoringSSL, traffic annotations, endpoint/wire-format thật (Chromium: updater/Safe Browsing/sync; Brave: Ads/Rewards/P3A, `api_request_helper`, OHTTP), HTTP vs HTTPS, TLS 1.3, h1/h2/h3 |
| [web_cookies_and_tokens.md](web_cookies_and_tokens.md) | Web cookies, session cookie, `HttpOnly` / `Secure` / `SameSite`, first-party vs third-party cookies, access token, refresh token, JWT, XSS, CSRF, browser storage |
| [browser_ads_interview_roadmap.md](browser_ads_interview_roadmap.md) | Roadmap ôn phỏng vấn Chromium + Blink + Ads Engine + Brave Shields: architecture, rendering pipeline, resource loading, JS/V8, DOM, ads pipeline, HTTP, Network Service, Mojo |

**Thứ tự đọc gợi ý:** `browser_ads_interview_roadmap.md` để nắm overview phỏng vấn → `browser_process.md` → `renderer_process.md` → `blink.md` → `network_process.md` → `web_cookies_and_tokens.md` để đào sâu.

---

## Related

- [../C++/ipc_concepts.md](../C++/ipc_concepts.md) — IPC cơ bản (pipes, shared memory, sockets) làm nền cho Mojo
- [../C++/multithreading_and_concurrency.md](../C++/multithreading_and_concurrency.md) — threading nền tảng cho task runner / thread pool trong Chromium
- [example/network_service/](example/network_service/) — ví dụ C++ mô phỏng Network Service: NetworkManager, singleton thread pool, HTTP over TLS over TCP/IP
- Chromium docs trong tree: `docs/process_model_and_site_isolation.md`, `docs/mojo_and_services.md`, `docs/render_document.md`, `third_party/blink/renderer/README.md`
