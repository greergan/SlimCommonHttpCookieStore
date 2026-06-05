#include <algorithm>
#include <format>
#include <sstream>
#include <vector>
#include <slim/common/http/cookie/store.h>

namespace slim::common::http {

namespace {

// Helper to trim leading and trailing whitespace from a string_view
std::string_view trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return str;
}

// Case-insensitive string comparison helper
bool iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != 
            std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

// Splits a string_view by a delimiter character
std::vector<std::string_view> split(std::string_view str, char delim) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = str.find(delim);

    while (end != std::string_view::npos) {
        // Only add the token if it is not empty
        if (start != end) {
            tokens.push_back(str.substr(start, end - start));
        }
        start = end + 1;
        end = str.find(delim, start);
    }

    // Handle the last token (after the last delimiter)
    if (start < str.size()) {
        tokens.push_back(str.substr(start));
    }

    return tokens;
}   

slim::SlimValue validate_expires(const std::string& s) {
    if (s.empty())
        return true;

    std::tm tm{};

    // RFC 1123: "Wed, 21 Oct 2015 07:28:00 GMT"
    std::istringstream ss{s};
    ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (!ss.fail())
        return true;

    // Obsolete RFC 850: "Wednesday, 21-Oct-15 07:28:00 GMT"
    std::istringstream ss_alt{s};
    ss_alt >> std::get_time(&tm, "%A, %d-%b-%y %H:%M:%S");
    if (!ss_alt.fail())
        return true;
    
    return slim::SlimValue{}.set_error(std::format("'{}' => invalid expires format", s));
}

slim::SlimValue validate_max_age(const std::string& s) {
    if (s.empty())
        return true;

    size_t start = (s[0] == '-') ? 1 : 0;
    if (start == s.size()) {
        return slim::SlimValue{}.set_error(std::format("'{}' => max-age must not be only a sign character", s));
    }

    if (!std::ranges::all_of(s.begin() + static_cast<std::string::difference_type>(start), s.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    })) {
        return slim::SlimValue{}.set_error(std::format("'{}' => max-age contains non-digit characters", s));
    }

    return true;
}

} // anonymous namespace

void slim::common::http::CookieStore::erase(std::string_view name, std::string_view domain, std::string_view path) {
    auto it = std::find_if(jar_.begin(), jar_.end(), [name, domain, path](const Cookie& c) {
        return c.name == name && c.domain == domain && c.path == path;
    });

    if (it != jar_.end()) {
        jar_.erase(it);
    }
}

const std::string slim::common::http::CookieStore::get(std::string_view name) const {
    auto it = std::find_if(jar_.begin(), jar_.end(), [name](const Cookie& c) {
        return c.name == name;
    });

    if (it != jar_.end()) {
        return it->value;
    }
    return {};
}

slim::SlimValue slim::common::http::CookieStore::set(Cookie&& cookie, const bool validate_expiry) {
    if(validate_expiry) {
        if (auto r = validate_expires(cookie.expires); !r)
            return r;

        if (auto r = validate_max_age(cookie.max_age); !r)
            return r;
    }

    auto it = std::find_if(jar_.begin(), jar_.end(),
        [&](const Cookie& c) { return c.name == cookie.name; });

    if (it != jar_.end())
        *it = std::move(cookie);
    else
        jar_.push_back(std::move(cookie));

    return true;
}

slim::SlimValue slim::common::http::CookieStore::set(std::string_view name, std::string_view value) {
    Cookie cookie;
    cookie.name  = std::string(trim(name));
    cookie.value = std::string(trim(value));
    return set(std::move(cookie));
}

slim::SlimValue slim::common::http::CookieStore::set(std::string_view string) {
    auto parts = split(string, ';');
    if (parts.empty())
        return slim::SlimValue{}.set_error("cookie string is empty");

    // First element contains the Name=Value pair
    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos)
        return slim::SlimValue{}.set_error(std::format("'{}' => malformed cookie: missing '='", string));

    Cookie cookie;
    cookie.name = std::string(trim(parts[0].substr(0, kv_sep)));
    cookie.value = std::string(trim(parts[0].substr(kv_sep + 1)));

    // Process additional directive tokens/flags
    for (size_t i = 1; i < parts.size(); ++i) {
        std::string_view part = trim(parts[i]);
        if (part.empty()) continue;

        size_t eq_pos = part.find('=');
        if (eq_pos == std::string_view::npos) {
            // Token is a standalone modifier flag
            if (iequals(part, "Secure")) cookie.secure = true;
            else if (iequals(part, "HttpOnly")) cookie.httponly = true;
            else if (iequals(part, "Partitioned")) cookie.partitioned = true;
        } else {
            // Token is a key-value attribute pair
            std::string_view key = trim(part.substr(0, eq_pos));
            std::string_view val = trim(part.substr(eq_pos + 1));

            if (iequals(key, "Domain")) cookie.domain = std::string(val);
            else if (iequals(key, "Expires")) cookie.expires = std::string(val);
            else if (iequals(key, "Max-Age")) cookie.max_age = std::string(val);
            else if (iequals(key, "Path")) cookie.path = std::string(val);
            else if (iequals(key, "SameSite")) cookie.same_site = std::string(val);
        }
    }

    return set(std::move(cookie));
}

slim::SlimValue slim::common::http::CookieStore::set_cookies(std::string_view sv) {
    auto pairs = split(sv, ';');

    for (std::string_view pair : pairs) {
        pair = trim(pair);
        if (pair.empty())
            continue;

        size_t sep = pair.find('=');
        if (sep == std::string_view::npos)
            return slim::SlimValue{false}.set_error(std::format("'{}' => malformed cookie pair: missing '='", sv));

        if (auto r = set(pair.substr(0, sep), pair.substr(sep + 1)); !r)
            return r;
    }

    return true;
}

std::string slim::common::http::CookieStore::serialize() {
    std::string serialized;
    if(!jar_.empty()) {
        serialized = "Cookie: ";
        for (size_t i = 0; i < jar_.size(); ++i) {
            if (i > 0)
                serialized += "; ";
            serialized += jar_[i].name + "=" + jar_[i].value;
        }
    }
    return serialized;
}

std::string slim::common::http::CookieStore::serialize_headers() {
    std::string headers;
    for (const auto& cookie : jar_) {
        headers += "Set-Cookie: " + cookie.name + "=" + cookie.value;

        if (!cookie.domain.empty())   headers += "; Domain="  + cookie.domain;
        if (!cookie.expires.empty())  headers += "; Expires=" + cookie.expires;
        if (!cookie.max_age.empty())  headers += "; Max-Age=" + cookie.max_age;
        if (!cookie.path.empty())     headers += "; Path="    + cookie.path;
        if (!cookie.same_site.empty()) headers += "; SameSite="+ cookie.same_site;

        if (cookie.secure)      headers += "; Secure";
        if (cookie.httponly)    headers += "; HttpOnly";
        if (cookie.partitioned) headers += "; Partitioned";

        headers += "\r\n";
    }
    return headers;
}

} // namespace slim::common::http