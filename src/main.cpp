#include <algorithm>
#include <ctime>
#include <format>
#include <memory>
#include <vector>

#include <slim/common/http/cookie.h>
#include <slim/common/http/cookie/store.h>

namespace slim::common::http {

namespace {

bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != static_cast<unsigned char>(rhs[i])) return false;
    return true;
}

std::string_view trim(std::string_view str) noexcept {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) str.remove_suffix(1);
    return str;
}

std::vector<std::string_view> split(std::string_view str, char delim) noexcept {
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

void CookieStore::erase(std::string_view name, std::string_view path, std::string_view domain) noexcept {
    // Use the erase-remove idiom to physically remove the element from the vector
    store.erase(
        std::remove_if(store.begin(), store.end(), [&](const std::shared_ptr<Cookie>& c) {
            // Match name (required)
            if (c->get_name() != name) return false;

            // Match path (handle optional/empty)
            auto c_path = c->get_path();
            if (!path.empty()) {
                if (!c_path.has_value() || *c_path != path) return false;
            }

            // Match domain (handle optional/empty)
            auto c_domain = c->get_domain();
            if (!domain.empty()) {
                if (!c_domain.has_value() || *c_domain != domain) return false;
            }

            return true;
        }),
        store.end()
    );
}

std::shared_ptr<Cookie> CookieStore::get(std::string_view name,
                                         std::string_view domain,
                                         std::string_view path) const noexcept {
    auto it = std::find_if(store.begin(), store.end(), [&](const std::shared_ptr<Cookie>& c) {
        if (c->get_name() != name) return false;

        if (!domain.empty()) {
            auto c_domain = c->get_domain();
            if (!c_domain.has_value() || *c_domain != domain) return false;
        }

        if (!path.empty()) {
            auto c_path = c->get_path();
            if (!c_path.has_value() || *c_path != path) return false;
        }

        return true;
    });

    return (it != store.end()) ? *it : nullptr;
}

CookieStatus CookieStore::set(Cookie&& cookie) noexcept {
    return set(std::make_shared<Cookie>(std::move(cookie)));
}

CookieStatus CookieStore::set(std::shared_ptr<Cookie> cookie) noexcept {
    auto it = std::find_if(store.begin(), store.end(), [&](const std::shared_ptr<Cookie>& c) {
        return *c == *cookie;
    });

    if (it != store.end()) *it = cookie;
    else store.push_back(cookie);

    return CookieStatus::OK;
}

CookieStatus CookieStore::set(std::string_view name, std::string_view value) noexcept {
    try {
        Cookie c(name, value);
        return set(std::move(c));
    } catch (const CookieException& e) {
        // Map your internal CookieStatus to your CookieStatus
        return CookieStatus::InvalidCookieName; // Or a more granular error
    }
}

CookieStatus CookieStore::set(std::string_view string) noexcept {
    auto parts = split(string, ';');
    if (parts.empty()) return CookieStatus::EmptyCookieString;

    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos) {
        return CookieStatus::MalformedCookieMissingEquals;
    }

    Cookie cookie;
    if (cookie.set_name(trim(parts[0].substr(0, kv_sep))) != CookieStatus::OK) return CookieStatus::InvalidCookieName;
    if (cookie.set_value(trim(parts[0].substr(kv_sep + 1))) != CookieStatus::OK) return CookieStatus::InvalidCookieValue;

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string_view part = trim(parts[i]);
        if (part.empty()) continue;

        size_t eq_pos = part.find('=');
        if (eq_pos == std::string_view::npos) {
            if (iequals(part, "secure")) cookie.set_secure("true");
            else if (iequals(part, "httponly")) cookie.set_httponly("true");
            else if (iequals(part, "partitioned")) cookie.set_partitioned("true");
        } else {
            std::string_view key = trim(part.substr(0, eq_pos));
            std::string_view val = part.substr(eq_pos + 1);

            if (iequals(key, "domain")) cookie.set_domain(val);
            else if (iequals(key, "expires")) cookie.set_expires(val);
            else if (iequals(key, "max-age")) cookie.set_max_age(val);
            else if (iequals(key, "path")) cookie.set_path(val);
            else if (iequals(key, "samesite")) cookie.set_same_site(val);
        }
    }
    return set(std::move(cookie));
}

CookieStatus CookieStore::set_cookies(std::string_view sv) noexcept {
    auto pairs = split(sv, ';');
    for (std::string_view pair : pairs) {
        if (pair.empty()) continue;

        size_t sep = pair.find('=');
        if (sep == std::string_view::npos) {
            return CookieStatus::MalformedCookiePairMissingEquals;
        }
        if (auto r = set(pair.substr(0, sep), pair.substr(sep + 1)); r != CookieStatus::OK) return r;
    }
    return CookieStatus::OK;
}

std::string CookieStore::serialize() {
    std::string headers;
    for (const auto& cookie : store) headers += cookie->serialize();
    return headers;
}

} // namespace slim::common::http
