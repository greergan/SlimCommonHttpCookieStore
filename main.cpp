#include <algorithm>
//#include <iomanip>
#include <vector>
#include <slim/common/http/cookie/store.h>

namespace slim::common::http {

namespace {

// Helper to trim leading and trailing whitespace from a string_view
std::string_view trim(std::string_view str) noexcept {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return str;
}

// Case-insensitive string comparison helper
bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
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
std::vector<std::string_view> split(std::string_view str, char delim) noexcept {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = str.find(delim);
    while (end != std::string_view::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delim, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// Validates that the expiration string can map to an actual time point
/* bool validate_expires_string(const std::string& expires_str) noexcept {
    if (expires_str.empty()) return true;
    
    std::tm tm = {};
    std::istringstream ss(expires_str);
    
    // Attempt to parse standard HTTP/RFC 1123 format (e.g., "Wed, 21 Oct 2015 07:28:00 GMT")
    ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (!ss.fail()) return true;
    
    // Fallback: Attempt to parse an alternate common format (e.g., "Wednesday, 21-Oct-15 07:28:00 GMT")
    std::istringstream ss_alt(expires_str);
    ss_alt >> std::get_time(&tm, "%d-%b-%y %H:%M:%S");
    
    return !ss_alt.fail();
} */

// Validates that the max-age string is a valid integer offset
bool validate_max_age_string(const std::string& max_age_str) noexcept {
    if (max_age_str.empty()) return true;
    
    // Ensure all characters are digits (allowing an optional leading minus sign)
    size_t start = (max_age_str[0] == '-') ? 1 : 0;
    if (start == max_age_str.size()) return false;
    
    return std::all_of(max_age_str.begin() + start, max_age_str.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
}

} // anonymous namespace

void slim::common::http::CookieStore::erase(std::string_view name) noexcept {
    auto it = std::find_if(jar_.begin(), jar_.end(), [name](const Cookie& c) {
        return c.name == cookie.name;
    });

    if (it != jar_.end()) {
        jar_.erase(it);
    }
}

std::string_view slim::common::http::CookieStore::get(std::string_view name) noexcept {
    auto it = std::find_if(jar_.begin(), jar_.end(), [name](const Cookie& c) {
        return c.name == cookie.name;
    });

    if (it != jar_.end()) {
        return it->value;
    }
    return {};
}

slim::SlimValue slim::common::http::CookieStore::set(Cookie cookie) noexcept {
/*     // 1. TypeError Enforcement: Both expires and max_age cannot coexist
    if (!cookie.expires.empty() && !cookie.max_age.empty()) {
        return slim::SlimValue{false}.set_error("TypeError: Both 'expires' and 'max_age' cannot be set concurrently.");
    }

    // 2. Time Conversion Validation
    if (!validate_expires_string(cookie.expires)) {
        return slim::SlimValue{false}.set_error("TypeError: Invalid 'expires' time format.");
    }
    if (!validate_max_age_string(cookie.max_age)) {
        return slim::SlimValue{false}.("TypeError: Invalid 'max_age' format.");
    } */

    // 3. Upsert into the jar (overwrite if matches existing name)
    auto it = std::find_if(jar_.begin(), jar_.end(), [&cookie](const Cookie& c) {
        return c.name == cookie.name;
    });

    if (it != jar_.end()) {
        *it = std::move(cookie);
    } else {
        jar_.push_back(std::move(cookie));
    }

    return slim::SlimValue(true);
}

slim::SlimValue slim::common::http::CookieStore::set(std::string_view string) noexcept {
    auto parts = split(string, ';');
    if (parts.empty()) return slim::SlimValue(false);

    // First element contains the Name=Value pair
    size_t kv_sep = parts[0].find('=');
    if (kv_sep == std::string_view::npos) return slim::SlimValue(false);

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
            else if (iequals(key, "SameSite")) cookie.SameSite = std::string(val);
        }
    }

    return set(std::move(cookie));
}

slim::SlimValue slim::common::http::CookieStore::set_cookies(std::string_view string) noexcept {
    // Parses a classic HTTP "Cookie:" header containing multiple key-value configurations
    auto pairs = split(string, ';');
    bool success = true;

    for (const auto& pair : pairs) {
        std::string_view trimmed_pair = trim(pair);
        if (trimmed_pair.empty()) continue;

        size_t sep = trimmed_pair.find('=');
        if (sep == std::string_view::npos) {
            success = false;
            continue;
        }

        auto res = set(trimmed_pair.substr(0, sep), trimmed_pair.substr(sep + 1));
        // If any variant internal error occurs, track it
        if (res.is_error()) success = false; 
    }

    return slim::SlimValue(success);
}

slim::SlimValue slim::common::http::CookieStore::set(std::string_view name, std::string_view value) noexcept {
    Cookie cookie;
    cookie.name = std::string(trim(name));
    cookie.value = std::string(trim(value));
    return set(std::move(cookie));
}

slim::SlimValue slim::common::http::CookieStore::serialize() noexcept {
    // Generates a traditional outbound cookie request header segment (e.g., "a=1; b=2")
    std::string serialized;
    for (size_t i = 0; i < jar_.size(); ++i) {
        if (i > 0) {
            serialized += "; ";
        }
        serialized += jar_[i].name + "=" + jar_[i].value;
    }
    return slim::SlimValue(serialized);
}

} // namespace slim::common::http