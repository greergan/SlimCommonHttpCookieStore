#include <catch2/catch_test_macros.hpp>
#include <slim/common/http/cookie.h>
#include <slim/common/http/cookie/store.h>

using slim::common::http::Cookie;
using slim::common::http::CookieStore;
using slim::common::http::ErrorStatus;

// ---------------------------------------------------------------------------
// set(name, value)
// ---------------------------------------------------------------------------

TEST_CASE("set(name, value) - basic insert and retrieval", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("session", "abc123") == ErrorStatus::OK);

    auto cookie = store.get("session");
    REQUIRE(cookie != nullptr);
    REQUIRE(cookie->get_value() == "abc123");
}

TEST_CASE("set(name, value) - trims whitespace from name and value", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("  token  ", "  xyz  ") == ErrorStatus::OK);
    auto cookie = store.get("token");
    REQUIRE(cookie != nullptr);
    REQUIRE(cookie->get_value() == "xyz");
}

TEST_CASE("set(name, value) - overwrites existing entry", "[cookie][set]") {
    CookieStore store;

    REQUIRE(store.set("x", "1") == ErrorStatus::OK);
    auto c1 = store.get("x");
    REQUIRE(c1 != nullptr);
    REQUIRE(c1->get_value() == "1");

    REQUIRE(store.set("x", "2") == ErrorStatus::OK);
    auto c2 = store.get("x");
    REQUIRE(c2 != nullptr);
    REQUIRE(c2->get_value() == "2");
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

    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);
    auto cookie = store.get("id");
    REQUIRE(cookie != nullptr);
    REQUIRE(cookie->get_value() == "42");
}

TEST_CASE("set(Cookie&&) - valid RFC 1123 expires is accepted", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c("a", "1");
    REQUIRE(c.set_expires("Wed, 21 Oct 2015 07:28:00 GMT") == ErrorStatus::OK);

    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);
    REQUIRE(store.get("a") != nullptr);
}

TEST_CASE("set(Cookie&&) - invalid expires string is rejected", "[cookie][set_cookie][expires]") {
    CookieStore store;

    Cookie c("a", "1");
    REQUIRE(c.set_expires("not-a-date") == ErrorStatus::CookieExpiresInvalidFormat);
}

TEST_CASE("set(Cookie&&) - valid max-age is accepted", "[cookie][set_cookie][max_age]") {
    CookieStore store;

    Cookie c("a", "1");
    REQUIRE(c.set_max_age(3600) == ErrorStatus::OK);

    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);
    REQUIRE(store.get("a") != nullptr);
}

// ---------------------------------------------------------------------------
// set(string_view) - Set-Cookie header parsing
// ---------------------------------------------------------------------------

TEST_CASE("set(string_view) - parses simple name=value", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("foo=bar") == ErrorStatus::OK);
    auto cookie = store.get("foo");
    REQUIRE(cookie != nullptr);
    REQUIRE(cookie->get_value() == "bar");
}

TEST_CASE("set(string_view) - parses Secure flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Secure") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_secure());
}

TEST_CASE("set(string_view) - parses HttpOnly flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; HttpOnly") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_httponly());
}

TEST_CASE("set(string_view) - parses Partitioned flag", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Partitioned") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_partitioned());
}

TEST_CASE("set(string_view) - parses Domain attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Domain=example.com") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_domain() == "example.com");
}

TEST_CASE("set(string_view) - parses Path attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Path=/api") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_path() == "/api");
}

TEST_CASE("set(string_view) - parses SameSite attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; SameSite=Strict") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_same_site() == "Strict");
}

TEST_CASE("set(string_view) - parses Max-Age attribute", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("s=1; Max-Age=3600") == ErrorStatus::OK);
    REQUIRE(!store.entries().empty());
    REQUIRE(store.entries().front()->get_max_age() == 3600);
}

TEST_CASE("set(string_view) - parses full attribute set", "[cookie][parse]") {
    CookieStore store;

    REQUIRE(store.set("id=99; Domain=example.com; Path=/; Secure; HttpOnly; SameSite=Lax") == ErrorStatus::OK);

    const auto& c = store.entries().front();
    REQUIRE(c != nullptr);
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

    REQUIRE(store.set("justtoken") == ErrorStatus::CookieMalformedMissingEquals);
}

// ---------------------------------------------------------------------------
// set_cookies(string_view) - Cookie request header parsing
// ---------------------------------------------------------------------------

TEST_CASE("set_cookies - parses single pair", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("a=1") == ErrorStatus::OK);
    auto cookie = store.get("a");
    REQUIRE(cookie != nullptr);
    REQUIRE(cookie->get_value() == "1");
}

TEST_CASE("set_cookies - parses multiple pairs", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("a=1; b=2; c=3") == ErrorStatus::OK);

    auto a = store.get("a");
    auto b = store.get("b");
    auto c = store.get("c");

    REQUIRE(a != nullptr); REQUIRE(a->get_value() == "1");
    REQUIRE(b != nullptr); REQUIRE(b->get_value() == "2");
    REQUIRE(c != nullptr); REQUIRE(c->get_value() == "3");
}

TEST_CASE("set_cookies - trims whitespace around pairs", "[cookie][set_cookies]") {
    CookieStore store;

    REQUIRE(store.set_cookies("  x = hello  ;  y = world  ") == ErrorStatus::OK);
    auto x = store.get("x");
    auto y = store.get("y");
    REQUIRE(x != nullptr); REQUIRE(x->get_value() == "hello");
    REQUIRE(y != nullptr); REQUIRE(y->get_value() == "world");
}

TEST_CASE("set_cookies - malformed pair returns error", "[cookie][set_cookies][error]") {
    CookieStore store;

    REQUIRE(store.set_cookies("a=1; badpair; c=3") == ErrorStatus::CookieMalformedPairMissingEquals);
}

// ---------------------------------------------------------------------------
// get
// ---------------------------------------------------------------------------

TEST_CASE("get - returns empty string for unknown name", "[cookie][get]") {
    CookieStore store;

    REQUIRE(store.get("missing") == nullptr);
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST_CASE("erase - removes an existing entry by name, path, and domain", "[cookie][erase]") {
    CookieStore store;

    Cookie c("k", "v");
    c.set_path("/");
    c.set_domain("example.com");
    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);

    store.erase("k", "example.com", "/");
    REQUIRE(store.get("k") == nullptr);
    REQUIRE(store.entries().empty());
}

TEST_CASE("erase - no-op for unknown name", "[cookie][erase]") {
    CookieStore store;

    REQUIRE(store.set("k", "v") == ErrorStatus::OK);
    store.erase("other", {}, {});
    REQUIRE(store.entries().size() == 1);
}

TEST_CASE("erase - does not remove cookie with different path", "[cookie][erase]") {
    CookieStore store;

    Cookie c("k", "v");
    c.set_path("/api");
    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);

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

    REQUIRE(store.set(Cookie("session", "abc123")) == ErrorStatus::OK);
    CHECK(store.serialize() == "Set-Cookie: session=abc123\r\n");
}

TEST_CASE("serialize - cookie with all optional attributes", "[cookie][serialize]") {
    CookieStore store;

    Cookie c("id", "42");
    c.set_domain("example.com");
    c.set_expires("Wed, 21 Oct 2015 07:28:00 GMT");
    c.set_path("/");
    c.set_same_site("None");
    c.set_secure(true);
    c.set_httponly(true);
    c.set_partitioned(true);
    REQUIRE(store.set(std::move(c)) == ErrorStatus::OK);

    const std::string expected =
        "Set-Cookie: id=42"
        "; Domain=example.com"
        "; Path=/"
        "; Expires=Wed, 21 Oct 2015 07:28:00 GMT"
        "; SameSite=None"
        "; Secure"
        "; HttpOnly"
        "; Partitioned"
        "\r\n";

    CHECK(store.serialize() == expected);
}

TEST_CASE("serialize - multiple cookies each get their own Set-Cookie line", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("foo", "1")) == ErrorStatus::OK);

    Cookie b("bar", "2");
    b.set_path("/api");
    REQUIRE(store.set(std::move(b)) == ErrorStatus::OK);

    const std::string result = store.serialize();
    CHECK(result.find("Set-Cookie: foo=1\r\n")          != std::string::npos);
    CHECK(result.find("Set-Cookie: bar=2; Path=/api\r\n") != std::string::npos);
}

TEST_CASE("serialize - boolean flags omitted when false", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("plain", "val")) == ErrorStatus::OK);

    const std::string result = store.serialize();
    CHECK(result.find("Secure")      == std::string::npos);
    CHECK(result.find("HttpOnly")    == std::string::npos);
    CHECK(result.find("Partitioned") == std::string::npos);
}

TEST_CASE("serialize - overwritten cookie reflects updated value", "[cookie][serialize]") {
    CookieStore store;

    REQUIRE(store.set(Cookie("token", "old")) == ErrorStatus::OK);

    Cookie c2("token", "new");
    c2.set_secure(true);
    REQUIRE(store.set(std::move(c2)) == ErrorStatus::OK);

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
    REQUIRE(store.set("a", "1") == ErrorStatus::OK);
    REQUIRE(store.entries().size() == 1);
    REQUIRE(store.set("b", "2") == ErrorStatus::OK);
    REQUIRE(store.entries().size() == 2);
}
