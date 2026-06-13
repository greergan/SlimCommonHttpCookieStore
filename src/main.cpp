#include <algorithm>
#include <ctime>
#include <format>
#include <memory>
#include <sstream>
#include <vector>
#include <slim/common/http/cookie/store.h>

namespace slim::common::http {

namespace {

std::string_view trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        str.remove_prefix(1);
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        str.remove_suffix(1);
    return str;
}

bool iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
            return false;
    }
    return true;
}

std::vector<std::string_view> split(std::string_view str, char delim) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = str.find(delim);
    while (end != std::string_view::npos) {
        if (start != end)
            tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delim, start);
    }
    if (start < str.size())
        tokens.push_back(str.substr(start));
    return tokens;
}

slim::SlimValue validate_expires(const std::string& s) {
    std::tm tm{};

    std::istringstream ss{s};
    ss.imbue(std::locale("C"));
    ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (!ss.fail())
        return true;

    std::istringstream ss_alt{s};
    ss_alt.imbue(std::locale("C"));
    ss_alt >> std::get_time(&tm, "%A, %d-%b-%y %H:%M:%S");
    if (!ss_alt.fail())
        return true;

    return slim::SlimValue{}.set_error(std::format("'{}' => invalid expires format", s));
}

} // anonymous namespace

void CookieStore::erase(std::string_view name, std::string_view path, std::string_view domain) {
    auto it = std::find_if(store.begin(), store.end(), [name, domain, path](const std::shared_ptr<Cookie>& c) {
        return c->name == name && c->domain == domain && c->path == path;
    });
    if (it != store.end())
        store.erase(it);
}

const std::string CookieStore::get(std::string_view name) const {
    auto it = std::find_if(store.begin(), store.end(), [name](const std::shared_ptr<Cookie>& c) {
        return c->name == name;
    });
    if (it != store.end())
        return (*it)->value;
    return {};
}

slim::SlimValue CookieStore::set(Cookie&& cookie, const bool validate_expiry) {
    if (!cookie.expires.empty() && !cookie.max_age.empty())
        return slim::SlimValue{false}.set_error("Use exactly one of either Expires or Max-Age");

    if (validate_expiry && !cookie.expires.empty()) {
        if (auto r = validate_expires(cookie.expires); !r)
            return r;
    }

    auto it = std::find_if(store.begin(), store.end(),
        [&](const std::shared_ptr<Cookie>& c) { return c->name == cookie.name && c->domain == cookie.domain && c->path == cookie.path; });

    if (it != store.end())
        **it = std::move(cookie);
    else
        store.push_back(std::make_shared<Cookie>(std::move(cookie)));

    return true;
}

slim::SlimValue CookieStore::set(std::string_view name, std::string_view value) {
    Cookie cookie;
    cookie.name  = std::string(trim(name));
    cookie.value = std::string(trim(value));
    return set(std::move(cookie));
}

slim::SlimValue CookieStore::set(std::string_view string) {
    auto parts = split(string, ';');
    if (parts.empty())
        return slim::SlimValue{}.set_error("cookie string is empty");

    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos)
        return slim::SlimValue{}.set_error(std::format("'{}' => malformed cookie: missing '='", string));

    Cookie cookie;
    cookie.name  = std::string(trim(parts[0].substr(0, kv_sep)));
    cookie.value = std::string(trim(parts[0].substr(kv_sep + 1)));

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string_view part = trim(parts[i]);
        if (part.empty()) continue;

        size_t eq_pos = part.find('=');
        if (eq_pos == std::string_view::npos) {
            if (iequals(part, "Secure"))           cookie.secure = true;
            else if (iequals(part, "HttpOnly"))    cookie.httponly = true;
            else if (iequals(part, "Partitioned")) cookie.partitioned = true;
        } else {
            std::string_view key = trim(part.substr(0, eq_pos));
            std::string_view val = trim(part.substr(eq_pos + 1));

            if (iequals(key, "Domain"))           cookie.domain    = std::string(val);
            else if (iequals(key, "Expires"))     cookie.expires   = std::string(val);
            else if (iequals(key, "Max-Age"))     cookie.max_age   = std::string(val);
            else if (iequals(key, "Path"))        cookie.path      = std::string(val);
            else if (iequals(key, "SameSite"))    cookie.same_site = std::string(val);
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
        if (sep == std::string_view::npos)
            return slim::SlimValue{false}.set_error(std::format("'{}' => malformed cookie pair: missing '='", sv));

        if (auto r = set(pair.substr(0, sep), pair.substr(sep + 1)); !r)
            return r;
    }
    return true;
}

std::string CookieStore::serialize_headers() {
    std::string headers;
    for (const auto& cookie : store) {
        headers += "Set-Cookie: " + cookie->name + "=" + cookie->value;

        if (!cookie->domain.empty())    headers += "; Domain="   + cookie->domain;
        if (!cookie->expires.empty())   headers += "; Expires="  + cookie->expires;
        if (!cookie->max_age.empty())   headers += "; Max-Age="  + cookie->max_age;
        if (!cookie->path.empty())      headers += "; Path="     + cookie->path;
        if (!cookie->same_site.empty()) headers += "; SameSite=" + cookie->same_site;

        if (cookie->secure)      headers += "; Secure";
        if (cookie->httponly)    headers += "; HttpOnly";
        if (cookie->partitioned) headers += "; Partitioned";

        headers += "\r\n";
    }
    return headers;
}

} // namespace slim::common::http
