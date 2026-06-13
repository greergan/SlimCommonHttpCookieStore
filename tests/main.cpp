#include <catch2/catch_test_macros.hpp>
#include <format>
#include <slim/common/http/cookie.h>
#include <slim/common/http/cookie/store.h>
#include <slim/SlimValue.hpp>

using slim::common::http::Cookie;
using slim::common::http::CookieStore;

// ---------------------------------------------------------------------------
// set(name, value)
// ---------------------------------------------------------------------------

TEST_CASE("set(name, value) - basic insert and retrieval", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("session", "abc123"));
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

    Cookie c("id", "42");
    c.set_path("/api");
    c.set_secure(true);

    REQUIRE(store.set(std::move(c)));
    REQUIRE(store.get("id") == "42");
}

TEST_CASE("set(Cookie&&) - valid RFC 1123 expires is accepted", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c("a", "1");
    c.set_expires("Wed, 21 Oct 2015 07:28:00 GMT");

    REQUIRE(store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - invalid expires string is rejected", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c("a", "1");
    c.set_expires("not-a-date");

    REQUIRE(!store.set(std::move(c)));
}

TEST_CASE("set(Cookie&&) - valid max-age is accepted", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c("a", "1");
    c.set_max_age(3600);

    REQUIRE(store.set(std::move(c)));
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
    REQUIRE(store.entries().front()->get_secure());
}

TEST_CASE("set(string_view) - parses HttpOnly flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; HttpOnly"));
    REQUIRE(store.entries().front()->get_httponly());
}

TEST_CASE("set(string_view) - parses Partitioned flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Partitioned"));
    REQUIRE(store.entries().front()->get_partitioned());
}

TEST_CASE("set(string_view) - parses Domain attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Domain=example.com"));
    REQUIRE(store.entries().front()->get_domain() == "example.com");
}

TEST_CASE("set(string_view) - parses Path attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Path=/api"));
    REQUIRE(store.entries().front()->get_path() == "/api");
}

TEST_CASE("set(string_view) - parses SameSite attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; SameSite=Strict"));
    REQUIRE(store.entries().front()->get_same_site() == "Strict");
}

TEST_CASE("set(string_view) - parses Max-Age attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Max-Age=3600"));
    REQUIRE(store.entries().front()->get_max_age() == 3600);
}

TEST_CASE("set(string_view) - parses full attribute set", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("id=99; Domain=example.com; Path=/; Secure; HttpOnly; SameSite=Lax"));

    const auto& c = store.entries().front();
    REQUIRE(c->get_name()      == "id");
    REQUIRE(c->get_value()     == "99");
    REQUIRE(c->get_domain()    == "example.com");
    REQUIRE(c->get_path()      == "/");
    REQUIRE(c->get_secure());
    REQUIRE(c->get_httponly());
    REQUIRE(c->get_same_site() == "Lax");
}

TEST_CASE("set(string_view) - missing '=' returns error", "[cookie][parse][error]") {
    CookieStore store;

    REQUIRE(!store.set("justtoken"));
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

    REQUIRE(!store.set_cookies("a=1; badpair; c=3"));
}

// ---------------------------------------------------------------------------
// get
// ---------------------------------------------------------------------------

TEST_CASE("get - returns empty string for unknown name", "[cookie][get]") {
    CookieStore store;

    REQUIRE(store.get("missing").empty());
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST_CASE("erase - removes an existing entry by name, path, and domain", "[cookie][erase]") {
    CookieStore store;

    Cookie c("k", "v");
    c.set_path("/");
    c.set_domain("example.com");
    REQUIRE(store.set(std::move(c)));

    store.erase("k", "/", "example.com");
    REQUIRE(store.get("k").empty());
    REQUIRE(store.entries().empty());
}

TEST_CASE("erase - no-op for unknown name", "[cookie][erase]") {
    CookieStore store;

    REQUIRE(store.set("k", "v"));
    store.erase("other", {}, {});
    REQUIRE(store.entries().size() == 1);
}

TEST_CASE("erase - does not remove cookie with different path", "[cookie][erase]") {
    CookieStore store;

    Cookie c("k", "v");
    c.set_path("/api");
    REQUIRE(store.set(std::move(c)));

    store.erase("k", "/", {});
    REQUIRE(store.entries().size() == 1);
}

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------

TEST_CASE("serialize - empty store returns empty string", "[cookie][serialize]") {
    CookieStore store;

    CHECK(store.serialize() == "");
}

TEST_CASE("serialize - single minimal cookie", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("session", "abc123")));
    CHECK(store.serialize() == "Set-Cookie: session=abc123\r\n");
}

TEST_CASE("serialize - cookie with all optional attributes", "[cookie][serialize]") {
    CookieStore store;

    Cookie c("id", "42");
    c.set_domain("example.com");
    c.set_expires("Wed, 21 Oct 2015 07:28:00 GMT");
    c.set_path("/");
    c.set_same_site("Strict");
    c.set_secure(true);
    c.set_httponly(true);
    c.set_partitioned(true);
    REQUIRE(store.set(std::move(c)));

    const std::string expected =
        "Set-Cookie: id=42"
        "; Domain=example.com"
        "; Expires=Wed, 21 Oct 2015 07:28:00 GMT"
        "; Path=/"
        "; SameSite=Strict"
        "; Secure"
        "; HttpOnly"
        "; Partitioned"
        "\r\n";

    CHECK(store.serialize() == expected);
}

TEST_CASE("serialize - multiple cookies each get their own Set-Cookie line", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("foo", "1")));

    Cookie b("bar", "2");
    b.set_path("/api");
    REQUIRE(store.set(std::move(b)));

    const std::string result = store.serialize();
    CHECK(result.find("Set-Cookie: foo=1\r\n")            != std::string::npos);
    CHECK(result.find("Set-Cookie: bar=2; Path=/api\r\n") != std::string::npos);
}

TEST_CASE("serialize - boolean flags omitted when false", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("plain", "val")));

    const std::string result = store.serialize();
    CHECK(result.find("Secure")      == std::string::npos);
    CHECK(result.find("HttpOnly")    == std::string::npos);
    CHECK(result.find("Partitioned") == std::string::npos);
}

TEST_CASE("serialize - overwritten cookie reflects updated value", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("token", "old")));

    Cookie c2("token", "new");
    c2.set_secure(true);
    REQUIRE(store.set(std::move(c2)));

    const std::string result = store.serialize();
    CHECK(result.find("token=old") == std::string::npos);
    CHECK(result.find("token=new") != std::string::npos);
    CHECK(result.find("Secure")    != std::string::npos);
}

// ---------------------------------------------------------------------------
// entries
// ---------------------------------------------------------------------------

TEST_CASE("entries - reflects current store contents", "[cookie][entries]") {
    CookieStore store;

    REQUIRE(store.entries().empty());
    REQUIRE(store.set("a", "1"));
    REQUIRE(store.entries().size() == 1);
    REQUIRE(store.set("b", "2"));
    REQUIRE(store.entries().size() == 2);
}
