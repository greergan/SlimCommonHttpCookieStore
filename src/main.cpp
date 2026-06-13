#include <algorithm>
#include <ctime>
#include <format>
#include <memory>
#include <vector>

#include <slim/common/http/cookie.h>
#include <slim/common/http/cookie/store.h>

#include <slim/SlimValue.hpp>

namespace slim::common::http {

namespace {

bool iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != static_cast<unsigned char>(rhs[i]))  return false;

    return true;
}

std::string_view trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))  str.remove_suffix(1);
    return str;
}
std::vector<std::string_view> split(std::string_view str, char delim) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = str.find(delim);
    while (end != std::string_view::npos) {
        if (start != end) tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delim, start);
    }
    if (start < str.size()) tokens.push_back(str.substr(start));
    return tokens;
}

} // anonymous namespace

void CookieStore::erase(std::string_view name, std::string_view domain, std::string_view path) {
    auto it = std::find_if(store.begin(), store.end(), [name, domain, path](const std::shared_ptr<Cookie>& c) {
        return c->name == name && c->path == path && c->domain == domain;
    });
    if (it != store.end()) store.erase(it);
}

const std::string CookieStore::get(std::string_view name) const {
    auto it = std::find_if(store.begin(), store.end(), [name](const std::shared_ptr<Cookie>& c) {
        return c->name == name;
    });
    if (it != store.end())
        return (*it)->value;
    return {};
}

slim::SlimValue CookieStore::set(Cookie&& cookie) {
    return set(std::make_shared<Cookie>(std::move(cookie)));
}

slim::SlimValue CookieStore::set(std::shared_ptr<Cookie> cookie) {
    auto it = std::find_if(store.begin(), store.end(), [&](const std::shared_ptr<Cookie>& c) {
        return *c == *cookie;
    });

    if (it != store.end()) *it = cookie;
    else store.push_back(cookie);

    return true;
}

slim::SlimValue CookieStore::set(std::string_view name, std::string_view value) {
    Cookie cookie(name, value);
    return set(std::move(cookie));
}

slim::SlimValue CookieStore::set(std::string_view string) {
    auto parts = split(string, ';');
    if (parts.empty()) return slim::SlimValue{}.set_error("cookie string is empty");

    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos) return slim::SlimValue{}.set_error(std::format("'{}' => malformed cookie: missing '='", string));

    Cookie cookie;
    COOKIE::STATUS e = cookie.set_name(trim(parts[0].substr(0, kv_sep)));
    if(e != COOKIE::STATUS::OK) return false;
    e = cookie.set_value(trim(parts[0].substr(kv_sep + 1)));
    if(e != COOKIE::STATUS::OK) return false;

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string_view part = trim(parts[i]);
        if (part.empty()) continue;

        size_t eq_pos = part.find('=');
        if (eq_pos == std::string_view::npos) {
            if (iequals(part, "secure"))           cookie.set_secure(true);
            else if (iequals(part, "httponly"))    cookie.set_httponly(true);
            else if (iequals(part, "partitioned")) cookie.set_partitioned(true);
        } else {
            std::string_view key = trim(part.substr(0, eq_pos));
            std::string_view val = trim(part.substr(eq_pos + 1));

            if (iequals(key, "domain"))           cookie.set_domain(val);
            else if (iequals(key, "expires"))     cookie.set_expires(val);
            else if (iequals(key, "max-age"))     cookie.set_max_age(val);
            else if (iequals(key, "path"))        cookie.set_path(val);
            else if (iequals(key, "samesite"))    cookie.set_same_site(val);
        }
    }

    return set(std::move(cookie));
}

slim::SlimValue CookieStore::set_cookies(std::string_view sv) {
    auto pairs = split(sv, ';');
    for (std::string_view pair : pairs) {
        pair = trim(pair);
        if (pair.empty()) continue;

        size_t sep = pair.find('=');
        if (sep == std::string_view::npos) return slim::SlimValue{false}.set_error(std::format("'{}' => malformed cookie pair: missing '='", sv));

        if (auto r = set(pair.substr(0, sep), pair.substr(sep + 1)); !r) return r;
    }
    return true;
}

std::string CookieStore::serialize() {
    std::string headers;
    for (const auto& cookie : store) headers += cookie->serialize();
    return headers;
}

} // namespace slim::common::http
