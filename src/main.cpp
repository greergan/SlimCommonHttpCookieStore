#include <algorithm>
#include <ctime>
#include <format>
#include <memory>
#include <vector>

#include <slim/common/http/cookie.h>
#include <slim/common/http/cookie/store.h>
#include <slim/common/utilities.h>

namespace slim::common::http {

void CookieStore::erase(std::string_view name, std::string_view domain, std::string_view path) noexcept {
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

ErrorStatus CookieStore::set(Cookie&& cookie) noexcept {
    return set(std::make_shared<Cookie>(std::move(cookie)));
}

ErrorStatus CookieStore::set(std::shared_ptr<Cookie> cookie) noexcept {
    auto it = std::find_if(store.begin(), store.end(), [&](const std::shared_ptr<Cookie>& c) {
        return *c == *cookie;
    });

    if (it != store.end()) *it = cookie;
    else store.push_back(cookie);

    return ErrorStatus::OK;
}

ErrorStatus CookieStore::set(std::string_view name, std::string_view value) noexcept {
    Cookie c;
    auto e = c.set_name(name);
    if (e != ErrorStatus::OK) return e;
    e = c.set_value(value);
    if (e != ErrorStatus::OK) return e;
    return set(std::move(c));
}

ErrorStatus CookieStore::set(std::string_view string) noexcept {
    auto parts = slim::common::utilities::split(string, ';');
    if (parts.empty()) return ErrorStatus::CookieEmptyString;

    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos) {
        return ErrorStatus::CookieMalformedMissingEquals;
    }

    Cookie cookie;
    if (cookie.set_name(parts[0].substr(0, kv_sep)) != ErrorStatus::OK) return ErrorStatus::CookieInvalidName;
    if (cookie.set_value(parts[0].substr(kv_sep + 1)) != ErrorStatus::OK) return ErrorStatus::CookieInvalidValue;

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string part;
        slim::common::utilities::trim(parts[i], part);
        if (part.empty()) continue;

        size_t eq_pos = part.find('=');
        if (eq_pos == std::string::npos) {
            if (slim::common::utilities::iequals(part, "secure")) cookie.set_secure("true");
            else if (slim::common::utilities::iequals(part, "httponly")) cookie.set_httponly("true");
            else if (slim::common::utilities::iequals(part, "partitioned")) cookie.set_partitioned("true");
        } else {
            std::string key;
            slim::common::utilities::trim(std::string_view(part).substr(0, eq_pos), key);
            std::string_view val = std::string_view(part).substr(eq_pos + 1);

            if (slim::common::utilities::iequals(key, "domain")) cookie.set_domain(val);
            else if (slim::common::utilities::iequals(key, "expires")) cookie.set_expires(val);
            else if (slim::common::utilities::iequals(key, "max-age")) cookie.set_max_age(val);
            else if (slim::common::utilities::iequals(key, "path")) cookie.set_path(val);
            else if (slim::common::utilities::iequals(key, "samesite")) cookie.set_same_site(val);
        }
    }
    return set(std::move(cookie));
}

ErrorStatus CookieStore::set_cookies(std::string_view sv) noexcept {
    auto pairs = slim::common::utilities::split(sv, ';');
    for (std::string_view pair : pairs) {
        if (pair.empty()) continue;

        size_t sep = pair.find('=');
        if (sep == std::string_view::npos) {
            return ErrorStatus::CookieMalformedPairMissingEquals;
        }
        if (auto r = set(pair.substr(0, sep), pair.substr(sep + 1)); r != ErrorStatus::OK) return r;
    }
    return ErrorStatus::OK;
}

std::string CookieStore::serialize() const {
    std::string headers;

    for (const auto& cookie : store)
        if (cookie) headers += cookie->serialize();

    return headers;
}

} // namespace slim::common::http
