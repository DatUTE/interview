# Browser Interview Roadmap: Chromium, Blink, Ads, Brave Shields

Tài liệu này là checklist ôn phỏng vấn cho vị trí liên quan đến Chromium, Blink,
Ads Engine hoặc browser internals. Mục tiêu là giúp trả lời nhanh các câu hỏi
kiểu "browser làm gì khi gặp `<script src=ads.js>`?", "Brave chặn ads ở đâu?",
"V8 có biết `document` không?", hoặc "vì sao Chromium dùng multi-process?".

**Cách học gợi ý:** đọc từ trên xuống một lần để nắm flow tổng thể, sau đó học
kỹ các pipeline ở mục Browser Architecture, Blink Rendering Pipeline, Resource
Loading, Ads Pipeline và Brave Shields.

---

## 1. Browser Architecture

Đây là phần nên nắm chắc nhất nếu CV có Chromium. Khi interviewer hỏi về
Chromium, họ thường muốn kiểm tra bạn hiểu process model và trust boundary.

### 1.1 Big Picture

```text
Browser Process
  |
  | Mojo IPC
  v
Renderer Process
  |  Blink + V8
  |
  +----> GPU Process
  |
  +----> Utility / Network Service
```

Chromium không chạy toàn bộ browser trong một process. Nó chia thành nhiều
process để tăng ổn định, bảo mật và khả năng song song.

### 1.2 Browser Process Làm Gì?

Browser process là process có quyền cao nhất.

Trách nhiệm chính:

- Quản lý UI: window, tab, omnibox, menu.
- Quản lý navigation: user nhập URL, click link, redirect.
- Quản lý các child process: renderer, GPU, utility, network service.
- Giữ dữ liệu nhạy cảm: cookie, password, history, profile, permission.
- Là trust anchor: renderer được xem là untrusted.
- Broker quyền: renderer muốn dùng file/network/storage phải đi qua browser hoặc service được browser cấp quyền.

Interview answer ngắn:

> Browser process là process privileged, chịu trách nhiệm UI, navigation, process
> management, security policy và brokering các capability nhạy cảm cho renderer.

### 1.3 Renderer Process Làm Gì?

Renderer process chạy web content.

Nó chứa:

- Blink: HTML/CSS/DOM/rendering engine.
- V8: JavaScript engine.
- Main thread: parse HTML, DOM, style, layout, paint, JS execution.
- Compositor thread: scroll/composite một số trường hợp không cần main thread.
- Worker threads: Web Worker, Service Worker, background JS.

Renderer là untrusted và sandboxed.

Interview answer ngắn:

> Renderer process chạy Blink và V8 để parse HTML/CSS, execute JS, build DOM,
> layout, paint và produce compositor data. Nó chạy untrusted web content trong
> sandbox nên không được trực tiếp truy cập tài nguyên nhạy cảm.

### 1.4 GPU Process Làm Gì?

GPU process xử lý phần graphics thấp hơn:

- Rasterization.
- GPU command execution.
- WebGL/WebGPU.
- Viz display compositor.
- Compositing frame cuối cùng ra màn hình.

Vì GPU driver có thể crash hoặc có bug security, Chromium tách nó ra process
riêng để giảm blast radius.

### 1.5 Utility Process Làm Gì?

Utility process chạy các service phụ, thường sandboxed theo từng loại service.

Ví dụ:

- Network Service.
- Audio service.
- Data decoder.
- Storage service.
- Printing.
- Unzip / file parsing.

Ý tưởng: code nào có rủi ro hoặc cần tách quyền thì chạy trong service/process
riêng.

### 1.6 Tại Sao Chromium Dùng Multi-Process?

Câu này gần như chắc chắn phải trả lời được.

Answer checklist:

- **Stability:** tab crash không kéo sập cả browser.
- **Security:** renderer chạy sandbox, browser giữ quyền cao.
- **Performance:** nhiều renderer chạy song song trên nhiều core.
- **Fault isolation:** GPU/network/utility crash có thể restart riêng.
- **Site Isolation:** site khác nhau có thể nằm ở process khác nhau.

Answer mẫu:

> Chromium dùng multi-process vì web content là untrusted. Tách browser,
> renderer, GPU và utility process giúp browser cô lập crash, áp sandbox cho
> renderer, tận dụng multi-core và triển khai Site Isolation để giảm rủi ro
> cross-site attack hoặc renderer compromise.

### 1.7 Site Isolation

Site Isolation là cơ chế tách các site khác nhau vào renderer process khác nhau.

Site thường được hiểu là:

```text
scheme + registrable domain
```

Ví dụ:

```text
https://a.example.com
https://b.example.com
```

cùng site là `https://example.com`, nhưng:

```text
https://example.com
https://evil.com
```

khác site.

Mục tiêu:

- Một renderer compromise không dễ đọc dữ liệu cross-site.
- Giảm rủi ro Spectre-class attacks.
- Cross-site iframe có thể chạy trong Out-of-Process iframe (OOPIF).

### 1.8 Mojo IPC

Mojo là IPC framework hiện đại của Chromium.

Mental model:

```text
Renderer
  |
  | Remote<T>
  v
Message Pipe
  |
  v
Receiver<T>
  |
Browser / Service
```

Renderer không gọi trực tiếp object trong browser process. Nó giữ `Remote`
interface, gửi message qua Mojo pipe, phía browser/service có `Receiver` nhận
và xử lý.

Interview answer ngắn:

> Mojo là hệ thống IPC của Chromium. Nó dùng message pipe và interface typed
> bằng mojom. Một process giữ `Remote`, process kia bind `Receiver`. Nhờ đó
> renderer có thể request service từ browser/network/GPU mà không cần direct
> memory access.

---

## 2. Blink Rendering Pipeline

Pipeline này rất quan trọng nếu phỏng vấn Ads Engine, DOM, rendering hoặc
performance.

### 2.1 Pipeline Từ HTML Đến Pixel

```text
HTML bytes
  |
  v
Tokenizer
  |
  v
DOM tree
  |
  +---- CSS bytes --> CSSOM
  |
  v
Style calculation
  |
  v
Layout
  |
  v
Paint
  |
  v
Composite
  |
  v
GPU / Viz
```

Giải thích từng bước:

- **HTML tokenizer:** biến bytes thành token HTML.
- **DOM:** tree gồm `Document`, `Element`, `Text`, `Node`.
- **CSSOM:** representation của CSS rules.
- **Style:** match selector, cascade, computed style.
- **Layout:** tính kích thước/vị trí box.
- **Paint:** record draw commands/display items.
- **Composite:** chia layer, combine layer, gửi compositor frame.
- **GPU/Viz:** raster/composite/display.

### 2.2 JavaScript Làm Pipeline Chạy Lại Như Thế Nào?

Khi JS modify DOM hoặc style:

```text
JS executes
  |
  v
Modify DOM / style
  |
  v
Invalidate style
  |
  v
Recalculate style
  |
  v
Layout if geometry changed
  |
  v
Paint if visual changed
  |
  v
Composite
```

Ví dụ:

```js
const div = document.createElement("div");
div.style.width = "100px";
document.body.appendChild(div);
```

Blink sẽ:

- Tạo DOM node.
- Gắn node vào tree.
- Mark document/style/layout dirty.
- Recalculate style.
- Layout node mới.
- Paint/composite frame mới.

### 2.3 Khi Nào Chỉ Composite, Khi Nào Layout?

Không phải mọi thay đổi đều trigger full pipeline.

Ví dụ thường gặp:

- `transform`, `opacity`: thường có thể composite-only.
- `color`, `background`: style + paint, thường không cần layout.
- `width`, `height`, `font-size`, `display`: style + layout + paint.
- DOM insertion/removal: thường style + layout + paint.

Interview answer ngắn:

> Blink không repaint toàn page cho mọi thay đổi. Nó dùng invalidation để xác
> định stage cần chạy lại. Một số property như transform/opacity có thể đi qua
> compositor, trong khi thay đổi geometry như width/height phải layout lại.

---

## 3. Resource Loading

Phần này liên quan trực tiếp đến Ads Engine và Brave Shields.

### 3.1 Khi Gặp `<script src="ads.js">`, Blink Làm Gì?

Flow đơn giản:

```text
HTML parser sees <script src="ads.js">
  |
  v
Blink requests resource
  |
  v
ResourceLoader / Fetch pipeline
  |
  v
Mojo URLLoaderFactory
  |
  v
Network Service
  |
  v
HTTP request to server
  |
  v
Response bytes
  |
  v
Blink receives script
  |
  v
V8 compiles/executes JS
```

Điểm cần nhớ:

- Blink phát hiện resource từ HTML/CSS/JS.
- Renderer không tự mở socket trực tiếp.
- Request đi qua Mojo sang Network Service.
- Browser/Network Service có thể áp policy, cache, cookie, CORS, filtering.

### 3.2 Blocking Script

Với script thường:

```html
<script src="ads.js"></script>
```

HTML parser có thể bị block cho đến khi script được download và execute.

Với:

```html
<script defer src="ads.js"></script>
<script async src="ads.js"></script>
```

behavior khác:

- `defer`: download song song, execute sau parse, giữ order.
- `async`: download song song, execute ngay khi tải xong, không giữ order.

### 3.3 Ads Engine Hay Can Thiệp Ở Đâu?

Các điểm can thiệp phổ biến:

- Trước network request: block URL `ads.js`.
- Sau response: inspect/modify response.
- Trước JS execute: block script.
- Sau JS execute: cosmetic filtering, remove DOM ad slots.
- Inject scriptlet: override API hoặc neutralize ad code.

---

## 4. JavaScript Execution

### 4.1 V8 Là Gì?

V8 là JavaScript engine:

- Parse JS.
- Compile JS.
- Execute JS.
- Manage JS heap and GC.
- Optimize hot code.

Nhưng V8 **không tự biết web platform**.

### 4.2 V8 Không Biết `document`

Đây là trap rất phổ biến.

V8 biết JavaScript language:

```js
let x = 1;
function f() {}
Promise.resolve();
```

Nhưng các object như:

```js
window
document
Element
Node
fetch
XMLHttpRequest
setTimeout
```

không phải core JavaScript. Chúng là Web APIs do Blink implement rồi expose vào
V8 thông qua bindings.

Mental model:

```text
JavaScript code
  |
  v
V8 executes JS
  |
  v
Blink bindings
  |
  v
Blink C++ objects: Window, Document, Element, Node
```

Interview answer mẫu:

> V8 executes JavaScript, but `document` is not part of ECMAScript. Blink
> implements DOM objects like Window, Document, Element, Node and exposes them
> to V8 through generated bindings from Web IDL.

### 4.3 Isolated Worlds

Chromium có khái niệm isolated world, thường dùng bởi extensions/content scripts.

Ý tưởng:

- Cùng DOM.
- JS global object/world tách biệt.
- Giúp extension chạy script mà không bị page script can thiệp trực tiếp.

Brave scriptlet/cosmetic logic cũng cần hiểu boundary này khi nói về injection.

---

## 5. DOM

DOM là object tree đại diện cho document.

### 5.1 Các API Cơ Bản Blink Làm Gì?

#### `document.createElement("div")`

Blink tạo một `Element` C++ object tương ứng với tag `div`.

Nó chưa xuất hiện trên page nếu chưa attach vào tree.

#### `appendChild(node)`

Blink:

- Validate node có thể insert không.
- Update parent/child relationship.
- Gắn node vào document tree nếu parent đang connected.
- Trigger mutation observer.
- Mark style/layout dirty.

#### `remove()`

Blink:

- Detach node khỏi parent.
- Update tree.
- Trigger mutation observer.
- Mark style/layout/paint invalidation nếu node đang rendered.

#### `querySelector(selector)`

Blink:

- Parse selector.
- Traverse/match DOM tree.
- Return first matching element.

### 5.2 DOM Không Phải Layout Tree

Quan trọng:

```text
DOM tree != Layout tree != Paint output
```

Ví dụ:

- `display: none` vẫn có DOM node nhưng không có layout box.
- pseudo-element có thể có rendering object nhưng không phải DOM element thật.
- Shadow DOM tạo thêm composed/flat tree concept.

Interview answer ngắn:

> DOM là semantic tree của document. Layout tree là rendering representation sau
> style calculation. Không phải mọi DOM node đều có layout object.

---

## 6. Ads Pipeline

Phần này nên đầu tư nhiều nếu role liên quan Ads Engine hoặc Brave.

### 6.1 Ads Script Pipeline

Ví dụ page có:

```html
<script src="https://ads.example.com/ads.js"></script>
```

Pipeline:

```text
HTML parser
  |
  v
Discover <script src=ads.js>
  |
  v
Download ads.js
  |
  v
Execute ads.js in V8
  |
  v
ads.js fetches ad metadata / bids
  |
  v
Network response: JSON / JS / HTML
  |
  v
JS creates ad DOM nodes
  |
  v
appendChild / innerHTML / iframe insertion
  |
  v
Style
  |
  v
Layout
  |
  v
Paint
```

### 6.2 Ads Có Thể Tạo UI Bằng Cách Nào?

Thường gặp:

- Inject `<iframe>`.
- Inject `<script>`.
- Create DOM elements.
- Use `innerHTML`.
- Load image/video.
- Use `fetch`/XHR to get ad JSON.
- Use tracking pixels.
- Use timers and event listeners.

### 6.3 Ads Engine Can Thiệp Ở Đâu?

```text
Before request
  |
  +-- Network filtering: block ads.js / tracking pixel

After DOM created
  |
  +-- Cosmetic filtering: hide/remove ad element

Before/while script runs
  |
  +-- Scriptlet injection: override APIs or neutralize known ad behavior
```

Nếu interviewer hỏi "block ads ở đâu tốt nhất?", câu trả lời nên có trade-off:

- Network filtering tiết kiệm bandwidth/CPU nhất.
- Cosmetic filtering xử lý được ad element render từ first-party hoặc dynamic DOM.
- Scriptlet injection xử lý anti-adblock/tracking logic nhưng rủi ro compatibility.

### 6.4 Example: Brave Block Ads Pipeline

```text
Page loads
  |
  v
HTML parser discovers ads resource
  |
  v
Network request classification
  |
  +-- Match block rule?
  |      |
  |      +-- yes -> cancel request
  |      +-- no  -> allow request
  |
  v
Page JS runs
  |
  v
DOM mutation creates ad slot
  |
  v
Cosmetic filter matches selector
  |
  v
Hide/remove element
```

---

## 7. Brave Shields

Brave Shields có thể được giải thích theo 3 lớp chính.

### 7.1 Network Filtering

Chặn request trước khi tải resource.

Ví dụ:

```text
https://ads.example.com/banner.js
https://tracker.example.com/pixel.gif
```

Ưu điểm:

- Tiết kiệm bandwidth.
- Tiết kiệm CPU vì script/image không được tải hoặc execute.
- Chặn tracking sớm.

Nhược điểm:

- Dễ break site nếu rule quá rộng.
- Không chặn được ad được serve từ first-party URL nếu không có rule phù hợp.
- Không xử lý hết DOM đã có sẵn trong HTML.

Answer ngắn:

> Network filtering là lớp hiệu quả nhất vì block trước khi resource được tải.
> Nó giảm bandwidth, CPU và tracking surface, nhưng phụ thuộc rule matching và
> có thể miss first-party/dynamic ads.

### 7.2 Cosmetic Filtering

Ẩn hoặc remove element sau khi DOM tồn tại.

Ví dụ rule:

```text
##.ad-banner
##[id^="sponsored"]
```

Ưu điểm:

- Xử lý được ad slot đã nằm trong DOM.
- Hữu ích khi request không thể block.
- Có thể sửa layout nhìn sạch hơn.

Nhược điểm:

- Resource có thể đã tải rồi, nên không tiết kiệm bandwidth.
- Cần selector matching/mutation observation.
- Có thể gây layout change hoặc hide nhầm content.

### 7.3 Scriptlet Injection

Inject JavaScript nhỏ để thay đổi behavior page.

Ví dụ:

- Override tracking API.
- Stub ad library function.
- Prevent anti-adblock detection.
- Modify `window` property.

Ưu điểm:

- Có thể xử lý logic phức tạp.
- Chặn anti-adblock hoặc tracking code không thể giải quyết bằng URL/selector.

Nhược điểm:

- Rủi ro compatibility cao hơn.
- Phải chạy đúng timing.
- Có thể ảnh hưởng behavior của page.

### 7.4 So Sánh 3 Lớp

| Layer | Chặn ở đâu | Ưu điểm | Nhược điểm |
| --- | --- | --- | --- |
| Network Filtering | Trước request | Tiết kiệm bandwidth/CPU, chặn sớm | Phụ thuộc rule, có thể miss first-party |
| Cosmetic Filtering | Sau DOM | Dọn UI, xử lý ad slot dynamic | Resource có thể đã tải |
| Scriptlet Injection | JS runtime | Xử lý anti-adblock/tracking logic | Dễ break site hơn |

Interview answer mẫu:

> Brave Shields thường kết hợp nhiều lớp. Network filtering block request sớm,
> cosmetic filtering xử lý phần tử còn sót trong DOM, còn scriptlet injection
> dùng cho anti-adblock hoặc tracking behavior khó chặn bằng URL/selector.

---

## 8. HTTP Cho Ads Engine

Ads Engine request rất nhiều, nên cần hiểu HTTP đủ tốt.

### 8.1 GET vs POST

**GET**

- Dùng để fetch resource hoặc query data.
- Params thường nằm trong URL.
- Có thể cache.
- Tracking pixel thường là GET.

**POST**

- Gửi body lên server.
- Dùng cho analytics, events, bidding, form.
- Thường ít cache hơn GET.

### 8.2 Cookie

Cookie dùng để identify user/session.

Relevant cho ads:

- Tracking.
- Frequency capping.
- Attribution.
- Login/session state.

Browser quản lý cookie trong Network Service/browser side, không phải renderer
tự đọc mọi cookie tùy ý.

### 8.3 Headers

Headers quan trọng:

- `User-Agent`
- `Accept`
- `Referer`
- `Origin`
- `Cookie`
- `Set-Cookie`
- `Cache-Control`
- `ETag`
- `If-None-Match`
- `Access-Control-Allow-Origin`

### 8.4 Cache-Control, ETag, 304

Flow:

```text
First request
  |
  v
200 OK + ETag: "abc"
  |
  v
Later request
  |
  v
If-None-Match: "abc"
  |
  v
304 Not Modified
```

Ý nghĩa:

- Server nói resource chưa đổi.
- Browser dùng cache.
- Giảm bandwidth.

### 8.5 CORS và Preflight

CORS kiểm soát cross-origin access từ JS.

Simple request có thể đi thẳng.

Non-simple request có preflight:

```text
OPTIONS /api
Origin: https://site.com
Access-Control-Request-Method: POST
Access-Control-Request-Headers: x-custom-header
```

Server phải trả header allow phù hợp.

Ads relevance:

- Ad script thường gọi nhiều cross-origin endpoint.
- CORS quyết định JS có đọc response được không.
- Request vẫn có thể được gửi, nhưng response có thể bị chặn khỏi JS nếu CORS fail.

### 8.6 Redirect

Ads/tracking hay dùng redirect chain:

```text
ad click
  |
  v
tracker A
  |
  v
tracker B
  |
  v
landing page
```

Browser/Network Service cần xử lý policy qua từng redirect, vì URL ban đầu có
thể clean nhưng redirect target là tracker/ad.

---

## 9. Network Service

Network Service là service chịu trách nhiệm networking trong Chromium.

### 9.1 Flow Renderer Đến Server

```text
Renderer Process
  |
  | Mojo URLLoaderFactory
  v
Browser Process / Policy
  |
  v
Network Service
  |
  v
DNS / Socket / TLS / HTTP cache / Cookies
  |
  v
Server
```

Renderer không trực tiếp mở TCP socket cho web request. Nó yêu cầu thông qua
Mojo interface, và Network Service thực hiện request.

### 9.2 Network Service Làm Gì?

- DNS.
- Socket.
- TLS.
- HTTP/2, HTTP/3.
- Cache.
- Cookie.
- Proxy.
- CORS integration.
- URLLoader.
- Redirect handling.

### 9.3 Vì Sao Tách Network Service?

Lý do:

- Security: cô lập networking code.
- Maintainability: networking thành service riêng.
- Policy enforcement: browser kiểm soát request.
- Crash isolation.

Interview answer ngắn:

> Renderer asks for network loads through Mojo. The actual socket, HTTP cache,
> cookies and redirect handling live in Network Service, which lets Chromium
> enforce security policy outside the untrusted renderer.

---

## 10. Mojo IPC

Không cần quá sâu, nhưng cần nói được flow.

### 10.1 Basic Flow

```text
Renderer
  |
  | owns mojo::Remote<SomeInterface>
  v
Message Pipe
  |
  v
Browser / Service
  |
  | owns mojo::Receiver<SomeInterface>
  v
Implementation object
```

### 10.2 Mojom

Chromium định nghĩa IPC interface bằng `.mojom`.

Ví dụ concept:

```text
interface URLLoaderFactory {
  CreateLoaderAndStart(...);
}
```

Build system generate C++ bindings cho both sides.

### 10.3 Vì Sao Mojo Quan Trọng?

- Typed IPC thay vì raw message.
- Rõ interface boundary.
- Có thể truyền handles/data pipes.
- Dùng rộng trong browser/renderer/network/GPU services.

### 10.4 Interview Answer

> Mojo là IPC layer của Chromium. Interfaces được định nghĩa bằng mojom. Client
> side giữ `Remote`, server side bind `Receiver`. Message đi qua pipe giữa
> process. Ví dụ renderer request network load thông qua `URLLoaderFactory`
> remote, còn Network Service xử lý ở receiver side.

---

## 11. End-to-End Scenario: Page Loads Ads

Đây là câu nên tự vẽ được trên bảng.

```text
User opens page
  |
  v
Browser process starts navigation
  |
  v
Network Service downloads HTML
  |
  v
Renderer receives HTML bytes
  |
  v
Blink tokenizer builds DOM
  |
  v
Parser sees <script src="ads.js">
  |
  v
Resource request via Mojo URLLoaderFactory
  |
  v
Network Service request
  |
  +-- Brave Network Filtering can block here
  |
  v
ads.js response
  |
  v
V8 executes script
  |
  v
Script fetches ad JSON / creates iframe / creates DOM nodes
  |
  +-- Brave Scriptlet Injection can affect JS behavior
  |
  v
DOM mutation
  |
  +-- Brave Cosmetic Filtering can hide/remove matched nodes
  |
  v
Style -> Layout -> Paint -> Composite -> GPU
```

### 11.1 Short Answer For Interview

> When Blink sees `<script src=ads.js>`, it requests the resource through the
> renderer loading stack and Mojo to Network Service. Brave can block the request
> at network filtering. If allowed, the script is passed to V8 for execution.
> The script may fetch ad data and mutate the DOM by creating elements or iframes.
> Those mutations invalidate style/layout/paint. Brave can still apply scriptlet
> injection during JS execution or cosmetic filtering after DOM nodes appear.

---

## 12. Common Interview Questions

### Q1. Why does Chromium use multiple processes?

Answer:

> For stability, security, performance, fault isolation and Site Isolation.
> Renderer processes run untrusted web content inside sandbox. Browser process
> stays privileged and validates/brokers sensitive operations. Site Isolation
> puts different sites into different renderer processes to reduce cross-site
> attack surface.

### Q2. What is the difference between Browser Process and Renderer Process?

Answer:

> Browser process manages UI, navigation, profiles, permissions and process
> coordination. Renderer process runs Blink and V8 for web content. Renderer is
> sandboxed and untrusted; browser is privileged and enforces policy.

### Q3. What happens after JS modifies DOM?

Answer:

> Blink updates the DOM tree, marks affected style/layout/paint state dirty, then
> on lifecycle update recalculates style, performs layout if geometry changed,
> records paint and commits compositor data.

### Q4. Does V8 implement `document`?

Answer:

> No. V8 implements JavaScript execution. `document`, `window`, `Element`,
> `fetch` and other Web APIs are implemented by Blink and exposed to V8 through
> Web IDL/generated bindings.

### Q5. Where can an ad blocker block ads?

Answer:

> At network request time by blocking ad/tracker URLs, at DOM time by cosmetic
> filtering, or at JS runtime by scriptlet injection. Network filtering is most
> efficient, cosmetic filtering cleans remaining UI, scriptlets handle complex
> anti-adblock/tracking behavior.

### Q6. What is Network Service?

Answer:

> Network Service owns actual network operations: sockets, DNS, TLS, HTTP cache,
> cookies, redirects and URLLoader. Renderer requests loads through Mojo instead
> of directly opening sockets.

### Q7. What is Mojo?

Answer:

> Mojo is Chromium's typed IPC system. Interfaces are described in mojom. One
> process uses a `Remote`, the other binds a `Receiver`, and messages flow
> through a message pipe.

### Q8. Why does Site Isolation matter for security?

Answer:

> It makes process boundaries align with web security boundaries. If a renderer
> for one site is compromised, it should not directly contain another site's
> document or sensitive data in the same address space.

---

## 13. One-Page Memory Map

```text
Browser Architecture:
  Browser = privileged coordinator
  Renderer = Blink + V8, sandboxed
  GPU = raster/composite/display
  Utility = isolated services
  Network Service = sockets/HTTP/cache/cookies
  Mojo = typed IPC
  Site Isolation = site-based process boundary

Blink Pipeline:
  HTML -> Tokenizer -> DOM
  CSS -> CSSOM
  DOM + CSSOM -> Style -> Layout -> Paint -> Composite -> GPU

JS:
  V8 executes JS
  Blink implements DOM/Web APIs
  JS DOM mutation -> invalidation -> style/layout/paint

Ads:
  script tag -> resource load -> execute JS -> fetch ad data
  -> create DOM/iframe -> layout/paint

Brave Shields:
  Network filtering = block request early
  Cosmetic filtering = hide/remove DOM element
  Scriptlet injection = alter JS behavior

HTTP:
  GET/POST, cookies, headers, cache-control, ETag/304, CORS, preflight, redirect
```

