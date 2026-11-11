#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace alist {

class JsonValue {
public:
    using Array  = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue, std::less<>>;

    JsonValue() noexcept : data_(nullptr) {}
    JsonValue(std::nullptr_t) noexcept : data_(nullptr) {}
    JsonValue(bool b) noexcept : data_(b) {}
    JsonValue(int v) noexcept : data_(static_cast<int64_t>(v)) {}
    JsonValue(unsigned int v) noexcept : data_(static_cast<int64_t>(v)) {}
    JsonValue(int64_t v) noexcept : data_(v) {}
    JsonValue(uint64_t v) noexcept : data_(static_cast<int64_t>(v)) {}
    JsonValue(double d) noexcept : data_(d) {}
    JsonValue(const char* s) : data_(std::string(s ? s : "")) {}
    JsonValue(std::string_view sv) : data_(std::string(sv)) {}
    JsonValue(std::string s) noexcept : data_(std::move(s)) {}
    JsonValue(Array arr) noexcept : data_(std::move(arr)) {}
    JsonValue(Object obj) noexcept : data_(std::move(obj)) {}

    static JsonValue array(std::initializer_list<JsonValue> init = {}) {
        return JsonValue(Array(init));
    }
    static JsonValue object(std::initializer_list<std::pair<const std::string, JsonValue>> init = {}) {
        return JsonValue(Object(init));
    }

    [[nodiscard]] bool is_null() const noexcept   { return std::holds_alternative<std::nullptr_t>(data_); }
    [[nodiscard]] bool is_bool() const noexcept   { return std::holds_alternative<bool>(data_); }
    [[nodiscard]] bool is_int() const noexcept    { return std::holds_alternative<int64_t>(data_); }
    [[nodiscard]] bool is_double() const noexcept { return std::holds_alternative<double>(data_); }
    [[nodiscard]] bool is_number() const noexcept { return is_int() || is_double(); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(data_); }
    [[nodiscard]] bool is_array() const noexcept  { return std::holds_alternative<Array>(data_); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<Object>(data_); }

    [[nodiscard]] bool as_bool(bool def = false) const noexcept {
        if (auto* p = std::get_if<bool>(&data_)) return *p;
        if (auto* p = std::get_if<int64_t>(&data_)) return *p != 0;
        return def;
    }
    [[nodiscard]] int64_t as_i64(int64_t def = 0) const noexcept {
        if (auto* p = std::get_if<int64_t>(&data_)) return *p;
        if (auto* p = std::get_if<double>(&data_)) return static_cast<int64_t>(*p);
        if (auto* p = std::get_if<bool>(&data_)) return *p ? 1 : 0;
        return def;
    }
    [[nodiscard]] double as_double(double def = 0.0) const noexcept {
        if (auto* p = std::get_if<double>(&data_)) return *p;
        if (auto* p = std::get_if<int64_t>(&data_)) return static_cast<double>(*p);
        return def;
    }
    [[nodiscard]] std::string as_str(std::string_view def = "") const {
        if (auto* p = std::get_if<std::string>(&data_)) return *p;
        return std::string(def);
    }
    [[nodiscard]] std::string_view as_sv(std::string_view def = "") const noexcept {
        if (auto* p = std::get_if<std::string>(&data_)) return *p;
        return def;
    }

    Array& as_array() {
        if (!is_array()) data_ = Array{};
        return std::get<Array>(data_);
    }
    const Array& as_array() const {
        static const Array kEmpty;
        if (auto* p = std::get_if<Array>(&data_)) return *p;
        return kEmpty;
    }

    Object& as_object() {
        if (!is_object()) data_ = Object{};
        return std::get<Object>(data_);
    }
    const Object& as_object() const {
        static const Object kEmpty;
        if (auto* p = std::get_if<Object>(&data_)) return *p;
        return kEmpty;
    }

    template <typename T>
    void push_back(T&& v) { as_array().emplace_back(std::forward<T>(v)); }

    JsonValue& operator[](std::string_view key) {
        auto& obj = as_object();
        auto it = obj.find(key);
        if (it != obj.end()) return it->second;
        return obj.emplace(std::string(key), JsonValue{}).first->second;
    }

    const JsonValue& get(std::string_view key) const noexcept {
        static const JsonValue kNull;
        if (auto* obj = std::get_if<Object>(&data_)) {
            auto it = obj->find(key);
            if (it != obj->end()) return it->second;
        }
        return kNull;
    }

    [[nodiscard]] bool contains(std::string_view key) const noexcept {
        if (auto* obj = std::get_if<Object>(&data_)) {
            return obj->find(key) != obj->end();
        }
        return false;
    }

    [[nodiscard]] size_t size() const noexcept {
        if (auto* a = std::get_if<Array>(&data_)) return a->size();
        if (auto* o = std::get_if<Object>(&data_)) return o->size();
        if (auto* s = std::get_if<std::string>(&data_)) return s->size();
        return 0;
    }

    [[nodiscard]] std::string dump(bool pretty = false) const;

    static std::optional<JsonValue> parse(std::string_view input, std::string* err_out = nullptr);

private:
    using Storage = std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object>;
    Storage data_;
    void dump_into(std::string& out, bool pretty, int indent) const;
};

std::string json_escape_string(std::string_view sv);

} // namespace alist
