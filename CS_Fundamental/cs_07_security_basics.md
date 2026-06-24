# Security Basics

## 1. Security Mindset

Security starts by asking:

- What asset are we protecting?
- Who is the attacker?
- What trust boundary is crossed?
- What can go wrong if input is malicious?
- What happens when credentials leak?

Interview answer:

> Security is not one feature. It is a set of constraints around identity, data, boundaries, and failure modes.

---

## 2. Authentication vs Authorization

Authentication answers:

> Who are you?

Authorization answers:

> What are you allowed to do?

Example:

- Login verifies identity.
- Role/permission check decides whether the user can delete a resource.

Common bug:

> Checking authentication but forgetting object-level authorization, such as allowing user A to access user B's invoice by changing an ID.

---

## 3. Hashing vs Encryption

Hashing:

- One-way.
- Fixed-size output.
- Used for integrity and password storage with salt/key stretching.

Encryption:

- Reversible with a key.
- Used for confidentiality.

Password storage:

- Do not store plaintext passwords.
- Do not use fast hashes like raw SHA-256 alone.
- Use password hashing algorithms such as bcrypt, scrypt, or Argon2.

---

## 4. Symmetric vs Asymmetric Crypto

Symmetric encryption:

- Same key encrypts and decrypts.
- Fast.
- Key distribution is hard.

Asymmetric encryption:

- Public/private key pair.
- Slower.
- Useful for key exchange, signatures, identity.

TLS uses both:

- Asymmetric crypto for authentication/key exchange.
- Symmetric crypto for bulk data encryption.

---

## 5. Input Validation

Treat external input as untrusted:

- User input.
- Network requests.
- Files.
- Environment variables.
- Third-party service responses.

Common risks:

- SQL injection.
- Command injection.
- Path traversal.
- Buffer overflow.
- Deserialization attacks.

Rule:

> Validate at trust boundaries and use structured APIs instead of string concatenation.

---

## 6. Secure Defaults

Good defaults:

- Deny by default.
- Least privilege.
- Short-lived tokens.
- TLS enabled.
- Secrets outside source code.
- Audit logs for sensitive operations.

Bad defaults:

- Admin by default.
- No rate limiting.
- Verbose error messages with secrets.
- Credentials in config files committed to git.

---

## 7. Threat Modeling

Simple threat modeling flow:

1. Draw system boundaries.
2. Identify assets.
3. Identify trust boundaries.
4. List attacker actions.
5. Add mitigations.
6. Decide residual risk.

Example:

```text
Mobile app -> API gateway -> auth service -> database
```

Trust boundaries:

- Device to internet.
- Internet to backend.
- Service to database.

---

## Interview Questions

- Authentication vs authorization?
- Hashing vs encryption?
- Why should passwords use bcrypt/scrypt/Argon2?
- What is least privilege?
- What is SQL injection?
- What is a trust boundary?
- Why should secrets not be committed to git?
