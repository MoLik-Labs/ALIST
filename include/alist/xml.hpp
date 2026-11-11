#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alist {

// Zero-XXE, depth-bounded XML DOM node for WebDAV (RFC 4918) and S3 XML responses.
struct XmlNode {
    std::string name; // Full tag name (e.g. "D:response" or "Contents")
    std::map<std::string, std::string> attrs;
    std::string text;
    std::vector<XmlNode> children;

    [[nodiscard]] std::string_view local_name() const noexcept {
        auto pos = name.find(':');
        return (pos == std::string::npos) ? std::string_view(name) : std::string_view(name).substr(pos + 1);
    }

    [[nodiscard]] const XmlNode* find_child(std::string_view local) const noexcept;
    [[nodiscard]] std::vector<const XmlNode*> find_children(std::string_view local) const;
    [[nodiscard]] std::string child_text(std::string_view local, std::string_view def = "") const;
};

std::string xml_escape(std::string_view sv);
std::string xml_unescape(std::string_view sv);
std::optional<XmlNode> parse_xml(std::string_view input, std::string* err_out = nullptr);

} // namespace alist
