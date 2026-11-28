#pragma once

#include "alist/json.hpp"
#include "alist/http_client.hpp"
#include "alist/common.hpp"
#include <algorithm>
#include <optional>
#include <string_view>

namespace alist {

inline bool valid_vt_sha256(std::string_view hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

// Caller validates both hash and key before constructing the request.
inline OutboundRequest make_vt_request(std::string_view hash, const std::string& key) {
    OutboundRequest request;
    request.url = "https://www.virustotal.com/api/v3/files/" + to_lower_ascii(hash);
    request.headers["x-apikey"] = key;
    request.connect_timeout_ms = 3000;
    request.timeout_ms = 10000;
    return request;
}

// Return only aggregate counts; do not expose filenames, vendor metadata or raw reports.
inline std::optional<JsonValue> parse_vt_report(std::string_view body) {
    auto root = JsonValue::parse(body);
    if (!root || !root->is_object()) return std::nullopt;
    const auto& attributes = root->get("data").get("attributes");
    const auto& stats = attributes.get("last_analysis_stats");
    if (!attributes.is_object() || !stats.is_object()) return std::nullopt;
    JsonValue result = JsonValue::object();
    for (const char* key : {"malicious", "suspicious", "harmless", "undetected", "timeout"}) {
        const auto& count = stats.get(key);
        if (!count.is_int() || count.as_i64() < 0 || count.as_i64() > 1000000) return std::nullopt;
        result[key] = count;
    }
    const auto& date = attributes.get("last_analysis_date");
    if (!date.is_int() || date.as_i64() < 0) return std::nullopt;
    result["last_analysis_date"] = date;
    return result;
}

} // namespace alist
