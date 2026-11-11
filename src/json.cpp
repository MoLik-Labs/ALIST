#include "alist/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace alist {

namespace {

constexpr int kMaxJsonDepth = 64;
constexpr size_t kMaxJsonBytes = 16 * 1024 * 1024;

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Parser {
public:
    explicit Parser(std::string_view sv) : sv_(sv) {}

    bool parse_root(JsonValue& out, std::string& err) {
        if (sv_.size() > kMaxJsonBytes) {
            err = "JSON payload exceeds maximum allowed size";
            return false;
        }
        skip_ws();
        if (!parse_value(out, 0, err)) return false;
        skip_ws();
        if (pos_ != sv_.size()) {
            err = "Trailing characters after JSON root value";
            return false;
        }
        return true;
    }

private:
    std::string_view sv_;
    size_t pos_ = 0;

    void skip_ws() noexcept {
        while (pos_ < sv_.size()) {
            char c = sv_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos_;
            else break;
        }
    }

    bool parse_hex4(uint16_t& val) noexcept {
        if (pos_ + 4 > sv_.size()) return false;
        uint16_t res = 0;
        for (int i = 0; i < 4; ++i) {
            unsigned char c = static_cast<unsigned char>(sv_[pos_++]);
            res <<= 4;
            if (c >= '0' && c <= '9') res |= (c - '0');
            else if (c >= 'a' && c <= 'f') res |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') res |= (c - 'A' + 10);
            else return false;
        }
        val = res;
        return true;
    }

    bool parse_string(std::string& out, std::string& err) {
        if (pos_ >= sv_.size() || sv_[pos_] != '"') {
            err = "Expected '\"'";
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < sv_.size()) {
            unsigned char c = static_cast<unsigned char>(sv_[pos_++]);
            if (c == '"') return true;
            if (c < 0x20) {
                err = "Unescaped control character in JSON string";
                return false;
            }
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (pos_ >= sv_.size()) {
                err = "Unterminated escape sequence";
                return false;
            }
            char esc = sv_[pos_++];
            switch (esc) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    uint16_t u1 = 0;
                    if (!parse_hex4(u1)) {
                        err = "Invalid \\uXXXX hex escape";
                        return false;
                    }
                    uint32_t cp = u1;
                    if (u1 >= 0xD800 && u1 <= 0xDBFF) {
                        if (pos_ + 2 <= sv_.size() && sv_[pos_] == '\\' && sv_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint16_t u2 = 0;
                            if (!parse_hex4(u2) || u2 < 0xDC00 || u2 > 0xDFFF) {
                                err = "Invalid UTF-16 low surrogate";
                                return false;
                            }
                            cp = 0x10000 + ((static_cast<uint32_t>(u1 - 0xD800) << 10) |
                                            static_cast<uint32_t>(u2 - 0xDC00));
                        } else {
                            err = "Unpaired UTF-16 high surrogate";
                            return false;
                        }
                    } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
                        err = "Unpaired UTF-16 low surrogate";
                        return false;
                    }
                    append_utf8(out, cp);
                    break;
                }
                default:
                    err = "Invalid escape character in JSON string";
                    return false;
            }
        }
        err = "Unterminated JSON string";
        return false;
    }

    bool parse_number(JsonValue& out, std::string& err) {
        size_t start = pos_;
        if (pos_ < sv_.size() && sv_[pos_] == '-') ++pos_;
        if (pos_ >= sv_.size()) {
            err = "Invalid number";
            return false;
        }
        if (sv_[pos_] == '0') {
            ++pos_;
        } else if (sv_[pos_] >= '1' && sv_[pos_] <= '9') {
            while (pos_ < sv_.size() && sv_[pos_] >= '0' && sv_[pos_] <= '9') ++pos_;
        } else {
            err = "Invalid number digit";
            return false;
        }
        bool is_float = false;
        if (pos_ < sv_.size() && sv_[pos_] == '.') {
            is_float = true;
            ++pos_;
            if (pos_ >= sv_.size() || sv_[pos_] < '0' || sv_[pos_] > '9') {
                err = "Expected digit after decimal point";
                return false;
            }
            while (pos_ < sv_.size() && sv_[pos_] >= '0' && sv_[pos_] <= '9') ++pos_;
        }
        if (pos_ < sv_.size() && (sv_[pos_] == 'e' || sv_[pos_] == 'E')) {
            is_float = true;
            ++pos_;
            if (pos_ < sv_.size() && (sv_[pos_] == '+' || sv_[pos_] == '-')) ++pos_;
            if (pos_ >= sv_.size() || sv_[pos_] < '0' || sv_[pos_] > '9') {
                err = "Expected digit in exponent";
                return false;
            }
            while (pos_ < sv_.size() && sv_[pos_] >= '0' && sv_[pos_] <= '9') ++pos_;
        }
        std::string_view token = sv_.substr(start, pos_ - start);
        if (!is_float) {
            int64_t iv = 0;
            auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), iv);
            if (ec == std::errc{} && p == token.data() + token.size()) {
                out = JsonValue(iv);
                return true;
            }
        }
        std::string tmp(token);
        char* endptr = nullptr;
        double dv = std::strtod(tmp.c_str(), &endptr);
        if (endptr != tmp.c_str() + tmp.size() || !std::isfinite(dv)) {
            err = "Numeric overflow or invalid float";
            return false;
        }
        out = JsonValue(dv);
        return true;
    }

    bool parse_value(JsonValue& out, int depth, std::string& err) {
        if (depth > kMaxJsonDepth) {
            err = "JSON maximum nesting depth exceeded";
            return false;
        }
        skip_ws();
        if (pos_ >= sv_.size()) {
            err = "Unexpected end of JSON input";
            return false;
        }
        char c = sv_[pos_];
        if (c == '"') {
            std::string s;
            if (!parse_string(s, err)) return false;
            out = JsonValue(std::move(s));
            return true;
        }
        if (c == '{') {
            ++pos_;
            JsonValue::Object obj;
            skip_ws();
            if (pos_ < sv_.size() && sv_[pos_] == '}') {
                ++pos_;
                out = JsonValue(std::move(obj));
                return true;
            }
            while (true) {
                skip_ws();
                std::string key;
                if (!parse_string(key, err)) return false;
                skip_ws();
                if (pos_ >= sv_.size() || sv_[pos_] != ':') {
                    err = "Expected ':' after object key";
                    return false;
                }
                ++pos_;
                JsonValue val;
                if (!parse_value(val, depth + 1, err)) return false;
                obj.insert_or_assign(std::move(key), std::move(val));
                skip_ws();
                if (pos_ >= sv_.size()) {
                    err = "Unterminated JSON object";
                    return false;
                }
                if (sv_[pos_] == '}') {
                    ++pos_;
                    break;
                }
                if (sv_[pos_] != ',') {
                    err = "Expected ',' or '}' in JSON object";
                    return false;
                }
                ++pos_;
            }
            out = JsonValue(std::move(obj));
            return true;
        }
        if (c == '[') {
            ++pos_;
            JsonValue::Array arr;
            skip_ws();
            if (pos_ < sv_.size() && sv_[pos_] == ']') {
                ++pos_;
                out = JsonValue(std::move(arr));
                return true;
            }
            while (true) {
                JsonValue elem;
                if (!parse_value(elem, depth + 1, err)) return false;
                arr.push_back(std::move(elem));
                skip_ws();
                if (pos_ >= sv_.size()) {
                    err = "Unterminated JSON array";
                    return false;
                }
                if (sv_[pos_] == ']') {
                    ++pos_;
                    break;
                }
                if (sv_[pos_] != ',') {
                    err = "Expected ',' or ']' in JSON array";
                    return false;
                }
                ++pos_;
            }
            out = JsonValue(std::move(arr));
            return true;
        }
        if (c == 't') {
            if (sv_.substr(pos_, 4) == "true") {
                pos_ += 4;
                out = JsonValue(true);
                return true;
            }
            err = "Invalid literal (expected true)";
            return false;
        }
        if (c == 'f') {
            if (sv_.substr(pos_, 5) == "false") {
                pos_ += 5;
                out = JsonValue(false);
                return true;
            }
            err = "Invalid literal (expected false)";
            return false;
        }
        if (c == 'n') {
            if (sv_.substr(pos_, 4) == "null") {
                pos_ += 4;
                out = JsonValue(nullptr);
                return true;
            }
            err = "Invalid literal (expected null)";
            return false;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number(out, err);
        }
        err = "Unexpected character in JSON";
        return false;
    }
};

} // namespace

std::string json_escape_string(std::string_view sv) {
    static constexpr const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(sv.size() + 8);
    out.push_back('"');
    for (unsigned char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<':  out += "\\u003c"; break; // Prevent HTML </script> breakout
            case '>':  out += "\\u003e"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0xF]);
                    out.push_back(kHex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

void JsonValue::dump_into(std::string& out, bool pretty, int indent) const {
    if (is_null()) {
        out += "null";
    } else if (auto* b = std::get_if<bool>(&data_)) {
        out += (*b ? "true" : "false");
    } else if (auto* iv = std::get_if<int64_t>(&data_)) {
        char buf[32];
        auto [p, _] = std::to_chars(buf, buf + sizeof(buf), *iv);
        out.append(buf, static_cast<size_t>(p - buf));
    } else if (auto* dv = std::get_if<double>(&data_)) {
        if (!std::isfinite(*dv)) {
            out += "null";
        } else {
            char buf[64];
            int n = std::snprintf(buf, sizeof(buf), "%.15g", *dv);
            if (n > 0) out.append(buf, static_cast<size_t>(n));
        }
    } else if (auto* sv = std::get_if<std::string>(&data_)) {
        out += json_escape_string(*sv);
    } else if (auto* arr = std::get_if<Array>(&data_)) {
        if (arr->empty()) {
            out += "[]";
            return;
        }
        out.push_back('[');
        for (size_t i = 0; i < arr->size(); ++i) {
            if (i > 0) out.push_back(',');
            if (pretty) {
                out.push_back('\n');
                out.append(static_cast<size_t>((indent + 1) * 2), ' ');
            }
            (*arr)[i].dump_into(out, pretty, indent + 1);
        }
        if (pretty) {
            out.push_back('\n');
            out.append(static_cast<size_t>(indent * 2), ' ');
        }
        out.push_back(']');
    } else if (auto* obj = std::get_if<Object>(&data_)) {
        if (obj->empty()) {
            out += "{}";
            return;
        }
        out.push_back('{');
        bool first = true;
        for (const auto& [k, v] : *obj) {
            if (!first) out.push_back(',');
            first = false;
            if (pretty) {
                out.push_back('\n');
                out.append(static_cast<size_t>((indent + 1) * 2), ' ');
            }
            out += json_escape_string(k);
            out.push_back(':');
            if (pretty) out.push_back(' ');
            v.dump_into(out, pretty, indent + 1);
        }
        if (pretty) {
            out.push_back('\n');
            out.append(static_cast<size_t>(indent * 2), ' ');
        }
        out.push_back('}');
    }
}

std::string JsonValue::dump(bool pretty) const {
    std::string out;
    out.reserve(128);
    dump_into(out, pretty, 0);
    return out;
}

std::optional<JsonValue> JsonValue::parse(std::string_view input, std::string* err_out) {
    Parser p(input);
    JsonValue v;
    std::string err;
    if (!p.parse_root(v, err)) {
        if (err_out) *err_out = std::move(err);
        return std::nullopt;
    }
    return v;
}

} // namespace alist
