#include <catch2/catch_test_macros.hpp>
#include <format>
#include <slim/common/http/cookie/store.h>
#include <slim/SlimValue.hpp>

using slim::common::http::Cookie;
using slim::common::http::CookieStore;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Cookie make_cookie(std::string name, std::string value) {
    Cookie c;
    c.name  = std::move(name);
    c.value = std::move(value);
    return c;
}

// ---------------------------------------------------------------------------
// set(name, value)
// ---------------------------------------------------------------------------

TEST_CASE("set(name, value) - basic insert and retrieval", "[cookie][set]") {
    CookieStore store;

    auto r = store.set("session", "abc123");
    REQUIRE(r);
    REQUIRE(store.get("session") == "abc123");
}

TEST_CASE("set(name, value) - trims whitespace from name and value", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("  token  ", "  xyz  "));
    REQUIRE(store.get("token") == "xyz");
}

TEST_CASE("set(name, value) - overwrites existing entry", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("x", "1"));
    REQUIRE(store.set("x", "2"));
    REQUIRE(store.get("x") == "2");
    REQUIRE(store.entries().size() == 1);
}

// ---------------------------------------------------------------------------
// set(Cookie&&)
// ---------------------------------------------------------------------------

TEST_CASE("set(Cookie&&) - inserts a well-formed cookie", "[cookie][set_cookie]") {
    CookieStore store;

    Cookie c = make_cookie("id", "42");
    c.path   = "/api";
    c.secure = true;

    REQUIRE(store.set(std::move(c)));
    REQUIRE(store.get("id") == "42");
}

TEST_CASE("set(Cookie&&) - valid RFC 1123 expires is accepted", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.expires   = "Wed, 21 Oct 2015 07:28:00 GMT";

    REQUIRE(store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - valid RFC 850 expires is accepted", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.expires   = "Wednesday, 21-Oct-15 07:28:00 GMT";

    REQUIRE(store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - invalid expires string is rejected", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.expires   = "not-a-date";

    auto r = store.set(std::move(c));
    REQUIRE(!r);
}

TEST_CASE("set(Cookie&&) - valid positive max-age is accepted", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.max_age   = "3600";

    REQUIRE(store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - valid negative max-age is accepted", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.max_age   = "-1";

    REQUIRE(store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - max-age with only a sign char is rejected", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.max_age   = "-";

    auto r = store.set(std::move(c));
    REQUIRE(!r);
}

TEST_CASE("set(Cookie&&) - max-age with non-digit chars is rejected", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.max_age   = "12x4";

    auto r = store.set(std::move(c));
    REQUIRE(!r);
}

TEST_CASE("set(Cookie&&) - validate_expiry=false skips validation", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c    = make_cookie("a", "1");
    c.expires   = "not-a-date";

    REQUIRE(store.set(std::move(c), false));
}

// ---------------------------------------------------------------------------
// set(string_view) - Set-Cookie header parsing
// ---------------------------------------------------------------------------

TEST_CASE("set(string_view) - parses simple name=value", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("foo=bar"));
    REQUIRE(store.get("foo") == "bar");
}

TEST_CASE("set(string_view) - parses Secure flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Secure"));
    REQUIRE(store.entries().front().secure);
}

TEST_CASE("set(string_view) - parses HttpOnly flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; HttpOnly"));
    REQUIRE(store.entries().front().httponly);
}

TEST_CASE("set(string_view) - parses Partitioned flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Partitioned"));
    REQUIRE(store.entries().front().partitioned);
}

TEST_CASE("set(string_view) - parses Domain attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Domain=example.com"));
    REQUIRE(store.entries().front().domain == "example.com");
}

TEST_CASE("set(string_view) - parses Path attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Path=/api"));
    REQUIRE(store.entries().front().path == "/api");
}

TEST_CASE("set(string_view) - parses SameSite attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; SameSite=Strict"));
    REQUIRE(store.entries().front().SameSite == "Strict");
}

TEST_CASE("set(string_view) - parses Max-Age attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Max-Age=3600"));
    REQUIRE(store.entries().front().max_age == "3600");
}

TEST_CASE("set(string_view) - parses full attribute set", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("id=99; Domain=example.com; Path=/; Secure; HttpOnly; SameSite=Lax"));

    auto c = store.entries().front();
    REQUIRE(c.name    == "id");
    REQUIRE(c.value   == "99");
    REQUIRE(c.domain  == "example.com");
    REQUIRE(c.path    == "/");
    REQUIRE(c.secure);
    REQUIRE(c.httponly);
    REQUIRE(c.SameSite == "Lax");
}

TEST_CASE("set(string_view) - attribute names are case-insensitive", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; secure; httponly; samesite=none"));

    auto c = store.entries().front();
    REQUIRE(c.secure);
    REQUIRE(c.httponly);
    REQUIRE(c.SameSite == "none");
}

TEST_CASE("set(string_view) - missing '=' returns error", "[cookie][parse][error]") {
    CookieStore store;

    auto r = store.set("justtoken");
    REQUIRE(!r);
}

// ---------------------------------------------------------------------------
// set_cookies(string_view) - Cookie request header parsing
// ---------------------------------------------------------------------------

TEST_CASE("set_cookies - parses single pair", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("a=1"));
    REQUIRE(store.get("a") == "1");
}

TEST_CASE("set_cookies - parses multiple pairs", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("a=1; b=2; c=3"));
    REQUIRE(store.get("a") == "1");
    REQUIRE(store.get("b") == "2");
    REQUIRE(store.get("c") == "3");
}

TEST_CASE("set_cookies - trims whitespace around pairs", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("  x = hello  ;  y = world  "));
    REQUIRE(store.get("x") == "hello");
    REQUIRE(store.get("y") == "world");
}

TEST_CASE("set_cookies - malformed pair returns error", "[cookie][set_cookies][error]") {
    CookieStore store;

    auto r = store.set_cookies("a=1; badpair; c=3");
    REQUIRE(!r);
}

// ---------------------------------------------------------------------------
// get
// ---------------------------------------------------------------------------

TEST_CASE("get - returns empty view for unknown name", "[cookie][get]") {
    CookieStore store;

    REQUIRE(store.get("missing").empty());
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST_CASE("erase - removes an existing entry", "[cookie][erase]") {
    CookieStore store;

    REQUIRE(store.set("k", "v"));
    store.erase("k");
    REQUIRE(store.get("k").empty());
    REQUIRE(store.entries().empty());
}

TEST_CASE("erase - no-op for unknown name", "[cookie][erase]") {
    CookieStore store;

    REQUIRE(store.set("k", "v"));
    store.erase("other");
    REQUIRE(store.entries().size() == 1);
}

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------

TEST_CASE("serialize - empty store returns empty string", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.serialize().empty());
}

TEST_CASE("serialize - single cookie", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set("a", "1"));
    REQUIRE(store.serialize() == "Cookie: a=1");
}

TEST_CASE("serialize - multiple cookies joined by '; '", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set("a", "1"));
    REQUIRE(store.set("b", "2"));
    REQUIRE(store.set("c", "3"));
    REQUIRE(store.serialize() == "Cookie: a=1; b=2; c=3");
}

TEST_CASE("serialize - round-trips set_cookies output", "[cookie][serialize]") {
    CookieStore store;

    const std::string_view header = "x=10; y=20; z=30";
    REQUIRE(store.set_cookies(header));
    REQUIRE(store.serialize() == std::format("Cookie: {}", header));
}

// ---------------------------------------------------------------------------
// entries
// ---------------------------------------------------------------------------

TEST_CASE("entries - reflects current jar contents", "[cookie][entries]") {
    CookieStore store;

    REQUIRE(store.entries().empty());
    REQUIRE(store.set("a", "1"));
    REQUIRE(store.entries().size() == 1);
    REQUIRE(store.set("b", "2"));
    REQUIRE(store.entries().size() == 2);
}