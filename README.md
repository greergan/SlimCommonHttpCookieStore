<a href="https://codeberg.org/greergan/SlimTS">
  <img src="https://raw.githubusercontent.com/greergan/SlimTS/master/assets/slimts_logo.png" width="75" alt="SlimTS Logo">
</a>   

# SlimCommonHttpCookieStore

Acts as a validating, deduplicating collection of [SlimCommonHttpCookie](https://codeberg.org/greergan/SlimCommonHttpCookie) instances.  
Used as the backing store for the [SlimTS](https://codeberg.org/greergan/SlimTS) Javascript CookieStore object.  
Part of the [SlimCommon](https://codeberg.org/greergan/SlimCommon) library.  
Dependency of the [SlimCommonHttpHeaders](https://codeberg.org/greergan/SlimCommonHttpHeaders) micro-library.  
Built using [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager).
CI/CD supplied by unified workflows provided by [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager).

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Core API](#core-api)
  - [CookieStore class](#cookiestore-class)
  - [Constructors and object lifetime](#constructors-and-object-lifetime)
  - [Setters](#setters)
  - [Getters and queries](#getters-and-queries)
  - [Serialization](#serialization)
  - [Design notes](#design-notes)
- [Building](#building)
- [Dependencies](#dependencies)
- [Workflows](#workflows)
- [Examples](#examples)

## Overview

This library provides a strict, validation-heavy collection type for managing multiple [`Cookie`](https://codeberg.org/greergan/SlimCommonHttpCookie) instances, with:

- RFC 6265 §5.3 compliant uniqueness — cookies are identified by name (case-sensitive), domain, and path
- Two independent parsing entry points for the two cookie-bearing HTTP headers (`Set-Cookie` vs `Cookie`)
- Replace-on-match semantics — setting a cookie that matches an existing entry's identity updates it in place rather than duplicating it
- Shared ownership of stored cookies via `std::shared_ptr<Cookie>`
- Status reporting via the same `CookieStatus` enum used by [`SlimCommonHttpCookie`](https://codeberg.org/greergan/SlimCommonHttpCookie)
- Heavy use of `noexcept`

[↑ Top](#table-of-contents)

## Features

| Feature | Description |
|--------|-------------|
| Cookie uniqueness | RFC 6265 §5.3 — identified by name (case-sensitive), domain, and path |
| Set-Cookie header parsing | Parses a single cookie plus its attributes (`name=value; Domain=...; Path=...; Secure; ...`) |
| Cookie header parsing | Parses a semicolon-delimited batch of `name=value` pairs with no attributes |
| Lookup | Retrieve a stored cookie by name, with optional domain/path narrowing |
| Removal | Erase a stored cookie by name, domain, and path |
| Replace-on-match | Setting a cookie matching an existing identity updates it in place |
| Serialize | Concatenates `Set-Cookie` headers for every stored cookie |

[↑ Top](#table-of-contents)

## Core API

### CookieStore class

```cpp
slim::common::http::CookieStore store;
```

[↑ Top](#table-of-contents)

### Constructors and object lifetime

| Form | Description |
|------|-------------|
| `CookieStore()` | Default constructor, produces an empty store |

[↑ Top](#table-of-contents)

### Setters

| Method | Description |
|--------|-------------|
| `CookieStatus set(std::string_view string) noexcept` | Parses a single `Set-Cookie`-style string — one cookie name/value plus its attributes — and stores it |
| `CookieStatus set(std::string_view name, std::string_view value) noexcept` | Constructs and stores a cookie directly from a name/value pair |
| `CookieStatus set(Cookie&& cookie) noexcept` | Takes ownership of an existing `Cookie` by move and stores it |
| `CookieStatus set(std::shared_ptr<Cookie> cookie) noexcept` | Stores a cookie via shared ownership |
| `CookieStatus set_cookies(std::string_view string) noexcept` | Parses a `Cookie`-request-header-style batch of semicolon-delimited `name=value` pairs and stores each one |
| `void erase(std::string_view name, std::string_view domain, std::string_view path) noexcept` | Removes the cookie matching the given name, domain, and path, if present |

[↑ Top](#table-of-contents)

### Getters and queries

| Method | Returns |
|--------|---------|
| `std::shared_ptr<Cookie> get(std::string_view name, std::string_view domain = {}, std::string_view path = {}) const noexcept` | The matching stored cookie, or `nullptr` if not found |
| `const std::vector<std::shared_ptr<Cookie>>& entries() const` | The full underlying collection of stored cookies |

[↑ Top](#table-of-contents)

### Serialization

```cpp
std::string CookieStore::serialize();
// -> concatenation of each stored cookie's "Set-Cookie: ...\r\n" header
```

Iterates the store and concatenates the `serialize()` output of every entry. Can throw if any stored cookie fails its own validation during serialization — see [`Cookie::serialize()`](https://codeberg.org/greergan/SlimCommonHttpCookie#serialization).

[↑ Top](#table-of-contents)

### Design notes

A couple of API decisions are intentional and worth calling out explicitly, since they could otherwise look like oversights:

**`set` vs. `set_cookies`.** These are not interchangeable, despite the shared prefix. `set` (in its string-parsing overload) consumes the shape of a single `Set-Cookie` *response* header — one cookie name/value followed by its attributes (`Domain`, `Path`, `Secure`, and so on). `set_cookies` consumes the shape of a `Cookie` *request* header — a flat, semicolon-delimited list of `name=value` pairs with no attributes at all. Reaching for the wrong one will parse without error but produce the wrong result (e.g. attribute-looking tokens passed to `set_cookies` get stored as literal cookie values, not interpreted as attributes), so the header you're consuming determines which method applies.

**`entries()` returns mutable `shared_ptr<Cookie>`.** This is deliberate. Callers may dereference an entry and call its non-`const` setters to mutate a stored cookie's attributes in place, without going through `CookieStore::set`. This intentionally bypasses `set`'s identity-matching and replace-on-match logic — it's the supported path for updating an existing cookie's attributes (e.g. flipping `Secure` or refreshing `Max-Age`) without that update being treated as a new, separate entry. Code that wants a read-only view should treat returned entries as read-only by convention rather than relying on the type system to enforce it.

[↑ Top](#table-of-contents)

## Building

This library is built using [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager). See that repository for build instructions.

[↑ Top](#table-of-contents)

## Dependencies

External package dependencies for this library are declared in the `required_packages` file at the repository root. This file is read by [SlimLibraryPackager](https://codeberg.org/greergan/SlimLibraryPackager) during the build process to resolve dependencies and install them if not present.

```
SlimCommonHttpCookie
```

[↑ Top](#table-of-contents)


## Workflows

Forgejo Workflows are provided by [SlimLibraryPackager](http://codeberg.org/greergan/SlimLibraryPackager)

[↑ Top](#table-of-contents)

## Examples

```cpp
// Parsing a Set-Cookie response header
slim::common::http::CookieStore store;

CookieStatus e = store.set("session=abc123; Path=/; Secure; SameSite=Strict");
if (e != CookieStatus::OK) return e;
```

```cpp
// Parsing a Cookie request header (multiple cookies, no attributes)
slim::common::http::CookieStore store;

CookieStatus e = store.set_cookies("session=abc123; theme=dark; locale=en-US");
if (e != CookieStatus::OK) return e;
```

```cpp
// Lookup and in-place mutation
auto cookie = store.get("session");
if (cookie) {
    cookie->set_secure(true);
    cookie->set_max_age(3600);
}
```

```cpp
// Removal
store.erase("session", "example.com", "/");
```

```cpp
// Serializing the whole store back into Set-Cookie headers
try {
    auto headers = store.serialize();
    // -> "Set-Cookie: session=abc123; Path=/; Secure\r\nSet-Cookie: theme=dark\r\n..."
}
catch (const slim::common::http::CookieException& e) {
    std::cerr << "Cookie serialization failed: " << e.what() << '\n';
}
```

[↑ Top](#table-of-contents)
