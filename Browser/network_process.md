# Chromium Network Process (Network Service) & Backend Communication

**Mục lục:** why a network process · the standard recipe (SimpleURLLoader) · under the hood (Mojo → //net → socket) · traffic annotations · real Chromium examples · Brave/fork specifics · HTTP vs HTTPS · TLS in Chromium · HTTP/1.1 vs h2 vs h3 · why vendor backends are HTTPS-only · Interview Q&A

**Siblings:** [browser_process.md](browser_process.md) · [renderer_process.md](renderer_process.md) · [blink.md](blink.md)

> Every source path below was verified against the live trees: Chromium at `main` (chromium.googlesource.com) and brave-core at `master` (github.com/brave/brave-core), July 2026.

---

## 1. The question this doc answers

*"How does the browser's own C++ code (not web pages) talk to its backend servers — component updates, Safe Browsing, sync, Brave Ads/Rewards? What protocol is on the wire?"*

**Short answer:** browser-core C++ never opens a socket. Every backend call goes through one funnel — `network::SimpleURLLoader` → Mojo IPC → the **Network Service** (its own sandboxed process) → the `//net` stack → a TLS socket (BoringSSL). The wire protocol is **HTTPS, always** — there is no plain-HTTP backend endpoint in either Chromium or Brave. The HTTP *version* (1.1, h2, h3/QUIC) is negotiated automatically and is invisible to the caller.

---

## 2. Why networking is a separate process

Chromium "servicified" its network stack into the **Network Service** (`services/network/`) — a Mojo service that runs **out-of-process in a dedicated utility process** on desktop (in-process on Android, on its own thread). Per `services/network/README.md`: *"If the network service crashes, it gets restarted in a new utility process"* — without taking the browser down.

| Reason | Explanation |
| --- | --- |
| **Single choke point** | One place owns the HTTP cache, cookie store, proxy resolution, TLS config, socket pools — per `NetworkContext` (profile vs incognito vs system) |
| **Sandboxing** | The process that parses untrusted network data is sandboxed; child processes (renderers) have *no* network access at all |
| **Fault isolation** | A crash in the network stack restarts one utility process, not the browser |
| **Auditability** | Every request funnels through Mojo `URLLoaderFactory`, and every request carries a traffic annotation (§5) |

`network::mojom::NetworkService` and `network::mojom::NetworkContext` are **trusted interfaces** — only the browser process may use them. Renderers get pre-configured, locked-down `URLLoaderFactory` pipes instead (see [renderer_process.md](renderer_process.md)).

```
Browser process                    Network Service process              Server
┌──────────────────┐   Mojo IPC   ┌────────────────────────────┐
│ Feature C++ code │─────────────▶│ URLLoaderFactory→URLLoader │
│ SimpleURLLoader  │              │  └▶ net::URLRequest        │
└──────────────────┘              │     └▶ URLRequestHttpJob   │
                                  │        └▶ HttpCache        │
┌──────────────────┐              │           └▶ HttpNetwork-  │   TLS 1.3
│ Renderer process │─────────────▶│              Transaction   │──────────▶ HTTPS
│ (locked factory) │   Mojo IPC   │              └▶ HttpStream-│   (h1/h2/h3)
└──────────────────┘              │                 Factory    │
                                  │                 └▶ SSLClientSocketImpl
                                  │                    (BoringSSL)          │
                                  └────────────────────────────┘
```

---

## 3. The standard recipe — how every feature calls a backend

Four steps, used by *every* browser-process feature (updater, Safe Browsing, sync, variations, Brave Ads/Rewards):

**Step 1 — get a `network::SharedURLLoaderFactory`.**

- Profile-scoped: `content::StoragePartition::GetURLLoaderFactoryForBrowserProcess()` (`content/public/browser/storage_partition.h`). Its header carries a SECURITY NOTE: this browser-process factory *relaxes* several protections (may disable ORB, no `request_initiator_origin_lock`/IsolationInfo) — so it must never serve web-origin-influenced requests.
- Profile-less/system: `SystemNetworkContextManager::GetSharedURLLoaderFactory()` (`chrome/browser/net/system_network_context_manager.h`) — a NetworkContext that stores nothing on disk and has no HTTP cache.

**Step 2 — build a `network::ResourceRequest`** — URL, method, headers, `load_flags`. System requests usually set `credentials_mode = network::mojom::CredentialsMode::kOmit` (no cookies).

**Step 3 — attach a `net::NetworkTrafficAnnotationTag`** — mandatory (§5).

**Step 4 — run `network::SimpleURLLoader`** (`services/network/public/cpp/simple_url_loader.h`). Its class comment: *"Wraps a URLLoader and runs it to completion. It's recommended that consumers use this class instead of URLLoader directly, due to the complexity of the API."*

```cpp
// Simplified — the pattern used by real features like the component updater.
auto request = std::make_unique<network::ResourceRequest>();
request->url = GURL("https://update.googleapis.com/service/update2/json");
request->method = "POST";
request->credentials_mode = network::mojom::CredentialsMode::kOmit;

net::NetworkTrafficAnnotationTag annotation =
    net::DefineNetworkTrafficAnnotation("update_client", R"(
      semantics { sender: "Component Updater" ... }
      policy    { cookies_allowed: NO ... })");

// The annotation is part of Create()'s signature — you cannot forget it.
auto loader = network::SimpleURLLoader::Create(std::move(request), annotation);
loader->AttachStringForUpload(post_data, "application/json");
loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
loader->DownloadToString(
    url_loader_factory.get(),
    base::BindOnce(&OnResponse),   // runs on the calling sequence
    /*max_body_size=*/1024 * 1024);
```

Useful `SimpleURLLoader` APIs: `DownloadToString(factory, callback, max_size)`, `DownloadToFile`, `DownloadHeadersOnly`, `AttachStringForUpload`, `SetRetryOptions(RETRY_ON_NETWORK_CHANGE)`, `SetAllowHttpErrorResults` (non-2xx bodies are dropped by default) — and the deliberately scary `DownloadToStringOfUnboundedSizeUntilCrashAndDie`.

---

## 4. Under the hood: Mojo → //net → socket

`net/docs/life-of-a-url-request.md` is the canonical walkthrough. The layering (every file verified at `main`):

| Layer | File / class | Responsibility |
| --- | --- | --- |
| Ergonomic wrapper | `network::SimpleURLLoader` | Retries, string/file download, size caps — browser side |
| Mojo boundary | `network::mojom::URLLoaderFactory` | Ships the `ResourceRequest` + a `URLLoaderClient` pipe to the network process |
| Network service | `services/network/url_loader.h` (`network::URLLoader`) | Owns one `net::URLRequest` per request |
| URL job | `net/url_request/url_request_http_job.cc` | Attaches cookies, starts the transaction |
| Cache | `net/http/http_cache_transaction.cc` | `HttpCache::Transaction` — serves from cache or wraps a network transaction |
| HTTP state machine | `net/http/http_network_transaction.cc` | `HttpNetworkTransaction::Start()` drives `STATE_CREATE_STREAM → ...` |
| Stream/version pick | `net/http/http_stream_factory.cc` | Chooses HTTP/1.1, h2 (SPDY session) or QUIC stream |
| Connection reuse | `net/socket/client_socket_pool_manager.cc` | Socket pools, `max_sockets_per_group` limits |
| TLS | `net/socket/ssl_client_socket_impl.cc` | Built directly on BoringSSL |

Key quote from `life-of-a-url-request.md`: child processes *"are generally sandboxed and have no network access themselves, apart from the network service"*. The browser process itself follows the same rule for HTTP — it marshals a `ResourceRequest` over Mojo; the socket lives in the network process.

---

## 5. Traffic annotations — every request is audited

`docs/network_traffic_annotations.md`: the goal is *"to have one object of this type or its variants as an argument of all functions that create a network request"*. Each `net::NetworkTrafficAnnotationTag` documents:

- **semantics** — sender, description, trigger, what data is sent, destination;
- **policy** — `cookies_allowed`, which user setting disables it, which enterprise `chrome_policy` turns it off.

Enforcement is real: `tools/traffic_annotation`'s `auditor.py` runs in presubmits/trybots, and `net/traffic_annotation/network_traffic_annotation.h` computes a `consteval` hash of the unique-id string at compile time. Interview point: **Chromium can answer "what does the browser send home, and how do I turn each flow off?" mechanically** — this is also what enterprise admins and privacy auditors rely on.

---

## 6. Real Chromium examples (verified endpoints & wire formats)

| Feature | Source | Endpoint | Wire format |
| --- | --- | --- | --- |
| Component/extension updates | `components/update_client`, `components/component_updater` | `https://update.googleapis.com/service/update2/json` | Omaha v3.1 — **JSON** POST |
| Safe Browsing v4 | `components/safe_browsing/core/browser/db` | `https://safebrowsing.googleapis.com/v4` | **Protobuf** in GET query |
| Safe Browsing v5 (realtime) | `.../hashprefix_realtime/hash_realtime_service.cc` | `https://safebrowsing.googleapis.com/v5/hashes:search` | Protobuf (+ OHTTP for IP privacy) |
| Variations / Finch | `components/variations` | `https://clientservices.googleapis.com/chrome-variations/seed` | Protobuf `VariationsSeed` |
| Sync | `components/sync` | `https://clients4.google.com/chrome-sync` | Protobuf `ClientToServerMessage` POST |
| Translate | `components/translate` | `https://translate.googleapis.com/translate_a/element.js` | Script download |

Details worth knowing:

- **Component updater**: endpoint constant `kUpdaterJSONDefaultUrl` in `components/component_updater/component_updater_url_constants.cc`; JSON built by `ProtocolSerializerJSON` (`components/update_client/protocol_serializer_json.cc` — root `"request"` dict with `protocol`, `acceptformat: "crx3,download,puff,run,xz,zucc"`, `sessionid`, ...). Transport in `components/update_client/net/network_impl.cc`: annotation `"update_client"`, POST via `SimpleURLLoader`, `RETRY_ON_NETWORK_CHANGE`. Anti-DDoS headers `X-Goog-Update-Updater/-AppId/-Interactivity`. Responses are signed — see §10 on why the channel is HTTPS anyway.
- **Safe Browsing v4** shows protobuf riding in a **GET query param**: `request.SerializeToString()` → `base::Base64UrlEncode` → `"%s/%s?$req=%s&$ct=application/x-protobuf"` (`v4_update_protocol_manager.cc`); exponential backoff capped at 24 h.
- **Variations** (`components/variations/service/variations_service.cc`): annotation `"chrome_variations_service"`, `credentials_mode = kOmit`, delta updates via `If-None-Match` plus `A-IM: x-bm,gzip`.
- **Sync** (`components/sync/engine/net/http_bridge.cc`): annotation `"sync_http_bridge"` with `cookies_allowed: NO`, `LOAD_BYPASS_CACHE | LOAD_DISABLE_CACHE`, protobuf from `components/sync/protocol/sync.proto`. Endpoint constants in `components/sync/base/sync_util.h`.

---

## 7. Brave specifics (applies to any Chromium fork)

Brave keeps Chromium's plumbing 100% and adds a wrapper + swaps endpoints. All findings from `brave/brave-core@master`.

### 7.1 `components/api_request_helper/` — Brave's standard wrapper

`APIRequestHelper` wraps `SimpleURLLoader` for Brave's REST APIs (used by Rewards browser-side, Brave News, Leo/AI Chat):

- **Cookie-less always**: `load_flags = net::LOAD_DO_NOT_SAVE_COOKIES`, `credentials_mode = kOmit`.
- **Retries**: 1× on network change (opt-in), never on 5xx.
- **Cache bypass** by default (`LOAD_BYPASS_CACHE | LOAD_DISABLE_CACHE` unless `enable_cache`); optional timeout and `max_body_size`.
- **JSON sanitization**: the body is re-parsed off the main thread with `base::JSONReader` (`components/api_request_helper/utils.cc`), so callers get a validated `base::Value`, never raw attacker-controlled bytes.
- `RequestSSE` for streaming responses (`network::SimpleURLLoaderStreamConsumer`) — used by Leo.

### 7.2 Brave Ads — transport-agnostic engine + Mojo delegation

The ads engine (`components/brave_ads/core/`) never fetches anything itself. It builds a **description** of the request — `mojom::UrlRequestInfo` (`components/brave_ads/core/mojom/brave_ads.mojom`: url, headers, content, method, `bool use_ohttp`) — and calls `AdsClient::UrlRequest()` (`components/brave_ads/core/public/ads_client/ads_client.h`). That crosses Mojo (`components/services/bat_ads/bat_ads.mojom`) to the browser-side `AdsServiceImpl::UrlRequest` (`components/brave_ads/browser/ads_service_impl.cc`), whose `HttpClient` (`components/brave_ads/core/browser/network/http_client.cc`) runs `SimpleURLLoader` with annotation `"brave_ads"`, `CredentialsMode::kOmit`. (The bat-ads service currently runs **in-process** on a background thread — `kInProcessBraveAdsServiceFeature`, enabled by default.)

Endpoints (`components/brave_ads/core/internal/common/url/request_builder/host/hosts/`, prod/staging switched on `mojom::EnvironmentType`):

| Host | Used for |
| --- | --- |
| `https://static.ads.brave.com` | Ad **catalog**: `GET /v9/catalog` |
| `https://anonymous.ads.brave.com` | Confirmations: `POST /v3/confirmation/{transaction_id}/{credential}` — blinded tokens (challenge-bypass-ristretto), plus token issuance/refill/redeem |
| `https://search.anonymous.ads.brave.com` | Search-result-ad confirmations |
| `https://mywallet.ads.brave.com` | Wallet-tied ads endpoint |
| `https://geo.ads.brave.com` | Geo lookup |

**Oblivious HTTP (2025+)**: non-reward confirmations set `use_ohttp = true` and go through the network service's `network::mojom::ObliviousHttpRequest` — HPKE key config from `https://static.ads.brave.com/v1/ohttp/hpkekeyconfig`, relay `https://ohttp.ads.brave.com/v1/ohttp/gateway` — so the gateway never sees the client IP.

### 7.3 Brave Rewards — a genuinely out-of-process engine

- `RewardsServiceImpl::StartEngineProcessIfNecessary` launches the Rewards engine in a **utility process** via `content::ServiceProcessHost::Launch` (`components/brave_rewards/content/rewards_service_impl.cc`).
- Endpoint classes live in `components/brave_rewards/core/engine/endpoints/`; hostnames in `engine/util/environment_config.cc`: `https://rewards.brave.com`, `https://api.rewards.brave.com`, `https://grant.rewards.brave.com`, `https://pcdn.brave.com`.
- Authenticated requests are **Ed25519-signed** (`engine/util/request_signer.cc`; key derived HKDF-SHA512 from the recovery seed, tweetnacl) — HTTP-signature-style `digest`/`signature` headers.
- Actual I/O crosses back to the browser: engine `URLLoader::Load` → mojom `RewardsEngineClient.LoadURL(UrlRequest) => (UrlResponse)` → `RewardsServiceImpl::LoadURL` → `SimpleURLLoader`. Same funnel as everything else.

### 7.4 Other Brave backend traffic

| Flow | Mechanism |
| --- | --- |
| **P3A analytics** (`components/p3a/`) | Constellation (Brave's STAR threshold aggregation, Rust via CXX). Randomness from `https://star-randsrv.bsg.brave.com` — server runs in an **AWS Nitro Enclave**, client verifies its remote-attestation document; encrypted reports POSTed to `https://collector.bsg.brave.com/{slow\|typical\|express\|creative}` |
| **Usage ping / referrals** | `browser/brave_stats/brave_stats_updater.cc` → `https://usage-ping.brave.com` (buildflag) |
| **Sync** | Chromium sync engine, server repointed via `chromium_src/components/sync/base/sync_util.h` override → `https://sync-v2.brave.com/v2`; E2E-encrypted client side (BIP39 sync chain, `components/brave_sync/crypto/`) |
| **Component/extension updates** | Chromium `update_client`, endpoint `https://go-updater.brave.com/extensions` (GN arg in `components/update_client/BUILD.gn`); `PrivacyPreservingProtocolHandlerFactory` strips fingerprintable Omaha fields; CUP signing disabled |
| **Proxied Google endpoints** | `safebrowsing2.brave.com`, `redirector.brave.com`, `clients4.brave.com`, ... (`components/constants/network_constants.h`) — network-delegate helper rewrites Google URLs to Brave-owned hosts and **forces `https`** |

### 7.5 Extra auth: the `BraveServiceKey` header

A build-time secret (`BUILDFLAG(BRAVE_SERVICES_KEY)`) sent as header `BraveServiceKey` to Brave services. Attached explicitly by services and automatically by `browser/net/brave_service_key_network_delegate_helper.cc` — but `ShouldAddBraveServicesKeyHeader()` (`components/constants/brave_services_key_helper.cc`) requires `url.SchemeIs(url::kHttpsScheme)` on `*.brave.com`/`*.bravesoftware.com`. **Structurally impossible to leak the key over plain HTTP.**

---

## 8. HTTP vs HTTPS — the protocol on the wire

**HTTPS is not a different protocol.** It is HTTP running inside TLS: `https://` scheme, default port **443** (vs 80). The HTTP semantics layer (methods, headers, status codes — RFC 9110) is byte-for-byte identical; only the transport changes.

**What TLS adds — three properties:**

| Property | Mechanism |
| --- | --- |
| **Confidentiality** | Application data encrypted with symmetric AEAD ciphers (AES-GCM, ChaCha20-Poly1305); keys from ephemeral ECDHE (X25519) → forward secrecy |
| **Integrity** | AEAD authentication tag — any on-path modification fails decryption |
| **Server authentication** | X.509 certificate chain to a trusted root + `CertificateVerify` signature over the handshake transcript |

**TLS 1.3 handshake (RFC 8446, 1-RTT):**

```
Client                                   Server
  │ ClientHello                            │
  │  (key_share, SNI, ALPN: h2,http/1.1)   │
  ├────────────────────────────────────────▶
  │            ServerHello (key_share)     │
  │            {EncryptedExtensions}       │
  │            {Certificate}               │
  │            {CertificateVerify}         │
  │            {Finished}                  │
  ◀────────────────────────────────────────┤
  │ {Finished}  + application data         │   ← 1 round trip
  ├────────────────────────────────────────▶
```

0-RTT early data is possible on session resumption (Chromium checks `SSL_in_early_data()` in `net/socket/ssl_client_socket_impl.cc`).

**Comparison table:**

| Dimension | HTTP | HTTPS |
| --- | --- | --- |
| Default port | 80 | 443 |
| Wire format | Plaintext HTTP over TCP | Same HTTP inside TLS records (or QUIC for h3) |
| Confidentiality | None — any on-path observer reads everything | AEAD encryption, forward secrecy |
| Integrity | None — trivially modifiable | AEAD tag; tampering breaks decryption |
| Authentication | None — trivially MITM'd | Cert chain (Chrome Root Store) + Certificate Transparency |
| Version negotiation | HTTP/1.1 only in practice | ALPN → h1/h2; Alt-Svc → h3 over QUIC |
| Browser treatment | "Not secure" chip; auto-upgraded; blocked as mixed content | Required for Service Workers, h2+, powerful web APIs |
| Performance | 0 extra RTT, capped at h1 | +1 RTT cold (TLS 1.3), 0-RTT resumed; h2 multiplexing; h3 removes TCP head-of-line blocking |

Honest caveat for interviews: "HTTPS costs a round trip" is only the cold-connection story — with TLS 1.3 resumption and QUIC the steady-state overhead is near zero, while HTTP forfeits all three security properties.

---

## 9. Where TLS lives in Chromium

| Concern | Location |
| --- | --- |
| TLS implementation | **BoringSSL** — `third_party/boringssl/` |
| TLS client socket | `net/socket/ssl_client_socket_impl.cc` — sets ALPN via `SSL_set_alpn_protos` |
| Cert verification | `net/cert/cert_verifier.h` |
| Root of trust | **Chrome Root Store** — `net/data/ssl/chrome_root_store/` (replaced OS trust stores starting Chrome 105) |
| Certificate Transparency | `net/docs/certificate-transparency.md` — missing SCTs ⇒ `ERR_CERTIFICATE_TRANSPARENCY_REQUIRED`, connection blocked |
| HSTS runtime | `net/http/transport_security_state.cc` — dynamic list, then static preload |
| HSTS preload list | `net/http/transport_security_state_static.json` (~10 MB, compiled into the binary) |
| HTTP/2 glue | `net/spdy/` (session management, flow control) |
| QUIC / HTTP/3 glue | `net/quic/`; the shared **QUICHE** library is DEPS-mapped to `net/third_party/quiche/src` (not `third_party/quiche`) |
| Alt-Svc memory | `net/http/http_server_properties.h` — remembers per-origin h3/h2 support |

**HTTP version selection is transparent to the caller**: ALPN inside the TLS handshake picks `h2` vs `http/1.1` on a TCP socket; HTTP/3 is discovered via Alt-Svc (and HTTPS DNS records) and runs over QUIC where TLS 1.3 is embedded in the transport handshake. The feature code that called `SimpleURLLoader` sees none of this.

---

## 10. Why vendor backends are HTTPS-only

Browser policy already pushes the web to HTTPS — HTTPS-First upgrades (`chrome/browser/ssl/https_upgrades_interceptor.cc`), mixed-content blocking (`third_party/blink/renderer/core/loader/mixed_content_checker.cc`), HSTS preload. For the browser's **own** backend traffic the bar is absolute, for two reasons:

1. **Privacy** — telemetry, stats, variations and sync payloads identify install cohorts and user state; they must not be readable or linkable by network observers. Note Brave layers *more* crypto on top of TLS: Ed25519 request signing (Rewards), blinded tokens (Ads), OHTTP relays (Ads confirmations), Constellation/STAR + Nitro-Enclave attestation (P3A) — TLS protects the pipe, these protect against the *server* linking users.
2. **Update integrity** — a plaintext HTTP update/component channel would let an on-path attacker substitute a malicious payload: a **supply-chain RCE against the entire user base**. TLS + cert verification + CT + package signing (Omaha responses are signed; CRX3 packages are signed) is the mitigation stack — defense in depth, so even the signed-payload layers still ride HTTPS.

---

## Interview Q&A

**Q1. How does browser-process C++ make a request to a backend server?**
Get a `SharedURLLoaderFactory` (from `StoragePartition` for profile requests, `SystemNetworkContextManager` for system requests), build a `network::ResourceRequest`, attach a mandatory `net::NetworkTrafficAnnotationTag`, run a `network::SimpleURLLoader`. The request crosses Mojo into the Network Service process, which owns the actual `net::URLRequest` and socket.

**Q2. Why does the network stack live in its own process?**
Sandboxing (the code parsing untrusted network input is isolated), fault isolation (crash → restart the utility process, not the browser), and a single choke point per `NetworkContext` for cache, cookies, proxy, TLS state and connection pooling. Child processes have no network access at all — renderers only hold locked-down `URLLoaderFactory` pipes.

**Q3. What is a NetworkTrafficAnnotationTag and why is it mandatory?**
A compile-time-hashed declaration attached to every request-creating call, documenting semantics (sender, trigger, data sent, destination) and policy (which user setting / enterprise policy disables the flow). Enforced by a presubmit auditor. It makes "what does the browser send home and how do I turn it off?" mechanically answerable.

**Q4. Walk from `SimpleURLLoader` down to the socket.**
`SimpleURLLoader` → Mojo `URLLoaderFactory` → `network::URLLoader` (network process) → `net::URLRequest` → `URLRequestHttpJob` (cookies) → `HttpCache::Transaction` → `HttpNetworkTransaction` (state machine) → `HttpStreamFactory` (picks h1/h2/QUIC) → socket pools → `SSLClientSocketImpl` on BoringSSL.

**Q5. Is the protocol HTTP or HTTPS? How do you know?**
HTTPS everywhere — the endpoint constants in source are literally `https://...` strings (`update.googleapis.com`, `safebrowsing.googleapis.com`, `static.ads.brave.com`, `api.rewards.brave.com`, ...). Brave even gates its `BraveServiceKey` auth header on `url.SchemeIs(kHttpsScheme)`, so plain HTTP to a Brave backend can't carry credentials at all.

**Q6. What exactly does HTTPS add over HTTP?**
HTTPS = HTTP over TLS. TLS adds confidentiality (AEAD symmetric encryption with forward-secret ECDHE keys), integrity (AEAD tag), and server authentication (X.509 chain + CT). HTTP semantics are unchanged; port 443 vs 80. TLS 1.3 needs one round trip cold, zero on resumption.

**Q7. Which HTTP version do backend calls use?**
Whatever ALPN/Alt-Svc negotiates — h2 is typical against Google/Brave frontends, h3/QUIC when advertised. `HttpStreamFactory` and `HttpServerProperties` handle it; the calling feature code can't tell and doesn't care.

**Q8. What wire formats ride on top?**
Mixed: Omaha updates are JSON; Safe Browsing, Variations and Sync are protobuf (Safe Browsing v4 even packs protobuf base64url into a GET query param); Brave's REST APIs are JSON. Transport is uniform, serialization is per-feature.

**Q9. How does Brave's ads engine fetch without touching the network itself?**
The engine builds a `mojom::UrlRequestInfo` and calls `AdsClient::UrlRequest`; the request crosses Mojo to browser-side `AdsServiceImpl`, which uses `SimpleURLLoader`. Same pattern for Rewards (whose engine runs in a real utility process and calls back via `RewardsEngineClient.LoadURL`). Keeps engines sandboxable and transport-agnostic.

**Q10. What is Oblivious HTTP and why does Brave use it?**
OHTTP encrypts the request to the gateway's HPKE key and sends it via a relay: the relay sees the client IP but not the content; the gateway sees content but not the IP. Brave uses it for non-reward ad confirmations (`ohttp.ads.brave.com` relay) so views can't be linked to users by IP.

**Q11. Why must the update channel be HTTPS if update payloads are already signed?**
Defense in depth plus metadata privacy. Signing stops payload substitution, but plaintext HTTP would leak which components/versions a user runs (fingerprinting, targeted exploitation of known-vulnerable versions) and allow downgrade/freeze attacks at the protocol level. An on-path attacker should get nothing — neither read nor write.

**Q12. Why can't a compromised renderer just use the browser's URLLoaderFactory?**
It never receives it. Renderers get factories minted by the browser with the origin lock and `IsolationInfo` baked in; the browser-process factory (which relaxes ORB/initiator locks) is explicitly documented as never to be exposed to web-influenced code. See [renderer_process.md](renderer_process.md) §security.

---

*Sources: `net/docs/life-of-a-url-request.md`, `services/network/README.md`, `docs/network_traffic_annotations.md`, `net/docs/certificate-transparency.md`, plus the per-feature files cited inline — all verified at Chromium `main` / brave-core `master`, July 2026.*
