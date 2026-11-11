#include "alist/xml.hpp"
#include "alist/common.hpp"

namespace alist {

namespace {

constexpr int kMaxXmlDepth = 48;
constexpr size_t kMaxXmlNodes = 100000;
constexpr size_t kMaxXmlBytes = 16 * 1024 * 1024;

class XmlParser {
public:
    explicit XmlParser(std::string_view sv) : sv_(sv) {}

    bool parse(XmlNode& root, std::string& err) {
        if (sv_.size() > kMaxXmlBytes) {
            err = "XML payload exceeds maximum allowed size";
            return false;
        }
        while (pos_ < sv_.size()) {
            skip_ws();
            if (pos_ >= sv_.size()) break;
            if (sv_.substr(pos_, 5) == "<?xml" || sv_.substr(pos_, 2) == "<?") {
                auto end = sv_.find("?>", pos_ + 2);
                if (end == std::string_view::npos) {
                    err = "Unterminated XML processing instruction";
                    return false;
                }
                pos_ = end + 2;
                continue;
            }
            if (sv_.substr(pos_, 4) == "<!--") {
                auto end = sv_.find("-->", pos_ + 4);
                if (end == std::string_view::npos) {
                    err = "Unterminated XML comment";
                    return false;
                }
                pos_ = end + 3;
                continue;
            }
            if (sv_.substr(pos_, 9) == "<!DOCTYPE" || sv_.substr(pos_, 8) == "<!ENTITY") {
                // Strictly reject DTD / ENTITY declarations to guarantee 100% XXE immunity!
                err = "XML DTD/ENTITY declarations are forbidden for security (XXE protection)";
                return false;
            }
            if (sv_[pos_] == '<') {
                return parse_element(root, 0, err);
            }
            ++pos_;
        }
        err = "Empty XML document";
        return false;
    }

private:
    std::string_view sv_;
    size_t pos_ = 0;
    size_t node_count_ = 0;

    void skip_ws() noexcept {
        while (pos_ < sv_.size()) {
            char c = sv_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos_;
            else break;
        }
    }

    static bool is_name_char(char c) noexcept {
        unsigned char u = static_cast<unsigned char>(c);
        return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') ||
               (u >= '0' && u <= '9') || c == '_' || c == '-' || c == ':' || c == '.';
    }

    bool parse_element(XmlNode& node, int depth, std::string& err) {
        if (depth > kMaxXmlDepth) {
            err = "XML maximum depth exceeded";
            return false;
        }
        if (++node_count_ > kMaxXmlNodes) {
            err = "XML maximum node count exceeded";
            return false;
        }
        if (pos_ >= sv_.size() || sv_[pos_] != '<') {
            err = "Expected '<'";
            return false;
        }
        ++pos_;
        size_t name_start = pos_;
        while (pos_ < sv_.size() && is_name_char(sv_[pos_])) ++pos_;
        if (pos_ == name_start) {
            err = "Empty XML tag name";
            return false;
        }
        node.name.assign(sv_.substr(name_start, pos_ - name_start));

        // Parse attributes
        while (pos_ < sv_.size()) {
            skip_ws();
            if (pos_ >= sv_.size()) {
                err = "Unterminated start tag";
                return false;
            }
            if (sv_[pos_] == '>') {
                ++pos_;
                break;
            }
            if (sv_.substr(pos_, 2) == "/>") {
                pos_ += 2;
                return true; // Self-closing
            }
            size_t ak_start = pos_;
            while (pos_ < sv_.size() && is_name_char(sv_[pos_])) ++pos_;
            if (pos_ == ak_start) {
                err = "Invalid XML attribute syntax";
                return false;
            }
            std::string attr_name(sv_.substr(ak_start, pos_ - ak_start));
            skip_ws();
            if (pos_ >= sv_.size() || sv_[pos_] != '=') {
                err = "Expected '=' in XML attribute";
                return false;
            }
            ++pos_;
            skip_ws();
            if (pos_ >= sv_.size() || (sv_[pos_] != '"' && sv_[pos_] != '\'')) {
                err = "Expected quoted attribute value";
                return false;
            }
            char quote = sv_[pos_++];
            size_t av_start = pos_;
            auto av_end = sv_.find(quote, av_start);
            if (av_end == std::string_view::npos) {
                err = "Unterminated XML attribute value";
                return false;
            }
            node.attrs[std::move(attr_name)] = xml_unescape(sv_.substr(av_start, av_end - av_start));
            pos_ = av_end + 1;
        }

        // Parse children & text
        std::string raw_text;
        while (pos_ < sv_.size()) {
            if (sv_[pos_] != '<') {
                size_t t_end = sv_.find('<', pos_);
                if (t_end == std::string_view::npos) {
                    err = "Unterminated XML element content";
                    return false;
                }
                raw_text.append(sv_.substr(pos_, t_end - pos_));
                pos_ = t_end;
                continue;
            }
            if (sv_.substr(pos_, 4) == "<!--") {
                auto c_end = sv_.find("-->", pos_ + 4);
                if (c_end == std::string_view::npos) {
                    err = "Unterminated XML comment";
                    return false;
                }
                pos_ = c_end + 3;
                continue;
            }
            if (sv_.substr(pos_, 9) == "<![CDATA[") {
                auto cd_end = sv_.find("]]>", pos_ + 9);
                if (cd_end == std::string_view::npos) {
                    err = "Unterminated CDATA section";
                    return false;
                }
                node.text.append(sv_.substr(pos_ + 9, cd_end - (pos_ + 9)));
                pos_ = cd_end + 3;
                continue;
            }
            if (sv_.substr(pos_, 2) == "<!") {
                err = "Forbidden XML declaration inside element";
                return false;
            }
            if (sv_.substr(pos_, 2) == "</") {
                pos_ += 2;
                size_t close_start = pos_;
                while (pos_ < sv_.size() && is_name_char(sv_[pos_])) ++pos_;
                std::string_view close_name = sv_.substr(close_start, pos_ - close_start);
                skip_ws();
                if (pos_ >= sv_.size() || sv_[pos_] != '>') {
                    err = "Malformed closing tag";
                    return false;
                }
                ++pos_;
                if (close_name != node.name) {
                    err = "Mismatched XML closing tag";
                    return false;
                }
                if (!raw_text.empty()) {
                    node.text += xml_unescape(raw_text);
                }
                return true;
            }
            XmlNode child;
            if (!parse_element(child, depth + 1, err)) return false;
            node.children.push_back(std::move(child));
        }
        err = "Unexpected EOF inside XML element";
        return false;
    }
};

} // namespace

const XmlNode* XmlNode::find_child(std::string_view local) const noexcept {
    for (const auto& c : children) {
        if (iequals_ascii(c.local_name(), local)) return &c;
    }
    return nullptr;
}

std::vector<const XmlNode*> XmlNode::find_children(std::string_view local) const {
    std::vector<const XmlNode*> res;
    for (const auto& c : children) {
        if (iequals_ascii(c.local_name(), local)) res.push_back(&c);
    }
    return res;
}

std::string XmlNode::child_text(std::string_view local, std::string_view def) const {
    if (const auto* c = find_child(local)) return c->text;
    return std::string(def);
}

std::string xml_escape(std::string_view sv) {
    std::string out;
    out.reserve(sv.size() + 16);
    for (char c : sv) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

std::string xml_unescape(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] != '&') {
            out.push_back(sv[i]);
            continue;
        }
        auto semi = sv.find(';', i + 1);
        if (semi == std::string_view::npos || semi - i > 10) {
            out.push_back('&');
            continue;
        }
        std::string_view ent = sv.substr(i + 1, semi - (i + 1));
        if (ent == "amp") out.push_back('&');
        else if (ent == "lt") out.push_back('<');
        else if (ent == "gt") out.push_back('>');
        else if (ent == "quot") out.push_back('"');
        else if (ent == "apos") out.push_back('\'');
        else if (!ent.empty() && ent[0] == '#') {
            uint64_t code = 0;
            bool ok = false;
            if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) {
                code = 0;
                ok = ent.size() > 2;
                for (size_t k = 2; k < ent.size() && ok; ++k) {
                    char ch = ent[k];
                    code <<= 4;
                    if (ch >= '0' && ch <= '9') code |= (ch - '0');
                    else if (ch >= 'a' && ch <= 'f') code |= (ch - 'a' + 10);
                    else if (ch >= 'A' && ch <= 'F') code |= (ch - 'A' + 10);
                    else ok = false;
                }
            } else {
                auto v = parse_u64(ent.substr(1));
                if (v) { code = *v; ok = true; }
            }
            if (ok && code > 0 && code <= 0x10FFFF) {
                if (code <= 0x7F) out.push_back(static_cast<char>(code));
                else if (code <= 0x7FF) {
                    out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else if (code <= 0xFFFF) {
                    out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xF0 | ((code >> 18) & 0x07)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
            } else {
                out.append(sv.substr(i, semi - i + 1));
            }
        } else {
            out.append(sv.substr(i, semi - i + 1));
        }
        i = semi;
    }
    return out;
}

std::optional<XmlNode> parse_xml(std::string_view input, std::string* err_out) {
    XmlParser p(input);
    XmlNode root;
    std::string err;
    if (!p.parse(root, err)) {
        if (err_out) *err_out = std::move(err);
        return std::nullopt;
    }
    return root;
}

} // namespace alist
