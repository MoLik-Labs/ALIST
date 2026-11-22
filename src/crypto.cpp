#include "alist/crypto.hpp"
#include "alist/common.hpp"
#include "alist/json.hpp"
#include "c_libs.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace alist {

namespace {

class EvpMdCtxGuard {
public:
    EvpMdCtxGuard() : ctx_(EVP_MD_CTX_new()) {}
    ~EvpMdCtxGuard() { if (ctx_) EVP_MD_CTX_free(ctx_); }
    EvpMdCtxGuard(const EvpMdCtxGuard&) = delete;
    EvpMdCtxGuard& operator=(const EvpMdCtxGuard&) = delete;
    EVP_MD_CTX* get() const noexcept { return ctx_; }
private:
    EVP_MD_CTX* ctx_;
};

class EvpCipherCtxGuard {
public:
    EvpCipherCtxGuard() : ctx_(EVP_CIPHER_CTX_new()) {}
    ~EvpCipherCtxGuard() { if (ctx_) EVP_CIPHER_CTX_free(ctx_); }
    EvpCipherCtxGuard(const EvpCipherCtxGuard&) = delete;
    EvpCipherCtxGuard& operator=(const EvpCipherCtxGuard&) = delete;
    EVP_CIPHER_CTX* get() const noexcept { return ctx_; }
private:
    EVP_CIPHER_CTX* ctx_;
};

} // namespace

bool random_bytes(uint8_t* out, size_t len) {
    if (len == 0) return true;
    while (len > 0) {
        int chunk = (len > 1048576) ? 1048576 : static_cast<int>(len);
        if (RAND_bytes(out, chunk) != 1) return false;
        out += chunk;
        len -= static_cast<size_t>(chunk);
    }
    return true;
}

std::string hex_encode(const uint8_t* data, size_t len) {
    static constexpr const char kHex[] = "0123456789abcdef";
    std::string out(len * 2, '\0');
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = kHex[data[i] >> 4];
        out[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

std::string random_hex(size_t num_bytes) {
    std::vector<uint8_t> buf(num_bytes);
    if (!random_bytes(buf.data(), buf.size())) return "";
    return hex_encode(buf.data(), buf.size());
}

std::string random_base64url(size_t num_bytes) {
    std::string raw(num_bytes, '\0');
    if (!random_bytes(reinterpret_cast<uint8_t*>(raw.data()), raw.size())) return "";
    return base64url_encode(raw);
}

bool constant_time_equals(std::string_view a, std::string_view b) noexcept {
    // Hash both operands with SHA-256 before CRYPTO_memcmp so neither secret content
    // nor secret length is leaked through early-return timing side-channels.
    const auto ha = sha256_bytes(a);
    const auto hb = sha256_bytes(b);
    const int diff = CRYPTO_memcmp(ha.data(), hb.data(), 32);
    if (diff != 0 || a.size() != b.size()) return false;
    return a.empty() || CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::array<uint8_t, 32> sha256_bytes(std::string_view data) {
    std::array<uint8_t, 32> out{};
    EvpMdCtxGuard ctx;
    unsigned int len = 0;
    if (ctx.get() &&
        EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) == 1) {
        EVP_DigestFinal_ex(ctx.get(), out.data(), &len);
    }
    return out;
}

std::string sha256_hex(std::string_view data) {
    auto d = sha256_bytes(data);
    return hex_encode(d.data(), d.size());
}

std::string md5_hex(std::string_view data) {
    std::array<uint8_t, 16> out{};
    EvpMdCtxGuard ctx;
    unsigned int len = 0;
    if (ctx.get() &&
        EVP_DigestInit_ex(ctx.get(), EVP_md5(), nullptr) == 1 &&
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) == 1) {
        EVP_DigestFinal_ex(ctx.get(), out.data(), &len);
    }
    return hex_encode(out.data(), out.size());
}

std::array<uint8_t, 32> hmac_sha256_bytes(std::string_view key, std::string_view data) {
    std::array<uint8_t, 32> out{};
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         out.data(), &len);
    return out;
}

std::string hmac_sha256_hex(std::string_view key, std::string_view data) {
    auto mac = hmac_sha256_bytes(key, data);
    return hex_encode(mac.data(), mac.size());
}

std::array<uint8_t, 20> hmac_sha1_bytes(std::string_view key, std::string_view data) {
    std::array<uint8_t, 20> out{};
    unsigned int len = 0;
    HMAC(EVP_sha1(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         out.data(), &len);
    return out;
}

std::string base64_encode(std::string_view bin) {
    static constexpr const char kB64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bin.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= bin.size()) {
        uint32_t v = (static_cast<uint32_t>(static_cast<uint8_t>(bin[i])) << 16) |
                     (static_cast<uint32_t>(static_cast<uint8_t>(bin[i + 1])) << 8) |
                     static_cast<uint32_t>(static_cast<uint8_t>(bin[i + 2]));
        out.push_back(kB64[(v >> 18) & 0x3F]);
        out.push_back(kB64[(v >> 12) & 0x3F]);
        out.push_back(kB64[(v >> 6) & 0x3F]);
        out.push_back(kB64[v & 0x3F]);
        i += 3;
    }
    if (i < bin.size()) {
        uint32_t v = static_cast<uint32_t>(static_cast<uint8_t>(bin[i])) << 16;
        if (i + 1 < bin.size()) v |= static_cast<uint32_t>(static_cast<uint8_t>(bin[i + 1])) << 8;
        out.push_back(kB64[(v >> 18) & 0x3F]);
        out.push_back(kB64[(v >> 12) & 0x3F]);
        out.push_back((i + 1 < bin.size()) ? kB64[(v >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::optional<std::string> base64_decode(std::string_view b64) {
    auto val_of = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::string out;
    out.reserve((b64.size() * 3) / 4);
    uint32_t buf = 0;
    int bits = 0;
    bool seen_pad = false;
    for (unsigned char c : b64) {
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        if (c == '=') {
            seen_pad = true;
            continue;
        }
        if (seen_pad) return std::nullopt;
        int v = val_of(c);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::string base64url_encode(std::string_view bin) {
    std::string s = base64_encode(bin);
    while (!s.empty() && s.back() == '=') s.pop_back();
    for (char& c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    return s;
}

std::optional<std::string> base64url_decode(std::string_view b64u) {
    return base64_decode(b64u);
}

std::string base32_encode(std::string_view bin) {
    static constexpr const char kB32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    out.reserve(((bin.size() + 4) / 5) * 8);
    uint32_t buffer = 0;
    int bits_left = 0;
    for (unsigned char c : bin) {
        buffer = (buffer << 8) | c;
        bits_left += 8;
        while (bits_left >= 5) {
            bits_left -= 5;
            out.push_back(kB32[(buffer >> bits_left) & 0x1F]);
        }
    }
    if (bits_left > 0) {
        out.push_back(kB32[(buffer << (5 - bits_left)) & 0x1F]);
    }
    return out;
}

std::optional<std::string> base32_decode(std::string_view b32) {
    std::string out;
    uint32_t buffer = 0;
    int bits_left = 0;
    for (unsigned char c : b32) {
        if (c == '=' || c == ' ') continue;
        int val = -1;
        if (c >= 'A' && c <= 'Z') val = c - 'A';
        else if (c >= 'a' && c <= 'z') val = c - 'a';
        else if (c >= '2' && c <= '7') val = c - '2' + 26;
        if (val < 0) return std::nullopt;
        buffer = (buffer << 5) | static_cast<uint32_t>(val);
        bits_left += 5;
        if (bits_left >= 8) {
            bits_left -= 8;
            out.push_back(static_cast<char>((buffer >> bits_left) & 0xFF));
        }
    }
    return out;
}

std::string hash_password_pbkdf2(std::string_view password, int iterations) {
    if (iterations < 10000) iterations = 210000;
    std::array<uint8_t, 16> salt{};
    random_bytes(salt.data(), salt.size());
    std::array<uint8_t, 32> dk{};
    PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                      salt.data(), static_cast<int>(salt.size()),
                      iterations, EVP_sha256(),
                      static_cast<int>(dk.size()), dk.data());
    std::string salt_b64 = base64url_encode(std::string_view(reinterpret_cast<const char*>(salt.data()), salt.size()));
    std::string dk_b64   = base64url_encode(std::string_view(reinterpret_cast<const char*>(dk.data()), dk.size()));
    OPENSSL_cleanse(dk.data(), dk.size());
    return "pbkdf2$sha256$" + std::to_string(iterations) + "$" + salt_b64 + "$" + dk_b64;
}

bool verify_password_pbkdf2(std::string_view password, std::string_view stored_hash) {
    auto parts = split_sv(stored_hash, '$');
    if (parts.size() != 5 || parts[0] != "pbkdf2" || parts[1] != "sha256") {
        return false;
    }
    auto iter_opt = parse_i64(parts[2]);
    if (!iter_opt || *iter_opt < 1000 || *iter_opt > 5000000) return false;
    auto salt_opt = base64url_decode(parts[3]);
    auto dk_expected_opt = base64url_decode(parts[4]);
    if (!salt_opt || !dk_expected_opt || dk_expected_opt->size() != 32) return false;

    std::array<uint8_t, 32> dk_actual{};
    int ok = PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                               reinterpret_cast<const unsigned char*>(salt_opt->data()),
                               static_cast<int>(salt_opt->size()),
                               static_cast<int>(*iter_opt),
                               EVP_sha256(),
                               static_cast<int>(dk_actual.size()), dk_actual.data());
    bool match = (ok == 1) && (CRYPTO_memcmp(dk_actual.data(), dk_expected_opt->data(), 32) == 0);
    OPENSSL_cleanse(dk_actual.data(), dk_actual.size());
    return match;
}

std::array<uint8_t, 32> derive_key_sha256(std::string_view master_secret, std::string_view context) {
    return hmac_sha256_bytes(master_secret, context);
}

std::optional<std::string> aes256_gcm_encrypt_with_iv(const std::array<uint8_t, 32>& key,
                                                      const std::array<uint8_t, 12>& iv,
                                                      std::string_view plaintext,
                                                      std::string_view aad) {
    EvpCipherCtxGuard ctx;
    if (!ctx.get()) return std::nullopt;
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return std::nullopt;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) return std::nullopt;
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1) return std::nullopt;

    int len = 0;
    if (!aad.empty()) {
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &len,
                              reinterpret_cast<const unsigned char*>(aad.data()),
                              static_cast<int>(aad.size())) != 1) {
            return std::nullopt;
        }
    }

    std::string out(plaintext.size() + 16, '\0');
    int cipher_len = 0;
    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx.get(),
                              reinterpret_cast<unsigned char*>(out.data()), &len,
                              reinterpret_cast<const unsigned char*>(plaintext.data()),
                              static_cast<int>(plaintext.size())) != 1) {
            return std::nullopt;
        }
        cipher_len = len;
    }
    if (EVP_EncryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char*>(out.data()) + cipher_len, &len) != 1) {
        return std::nullopt;
    }
    cipher_len += len;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16,
                            reinterpret_cast<unsigned char*>(out.data()) + cipher_len) != 1) {
        return std::nullopt;
    }
    out.resize(static_cast<size_t>(cipher_len) + 16);
    return out;
}

std::optional<std::string> aes256_gcm_decrypt_with_iv(const std::array<uint8_t, 32>& key,
                                                      const std::array<uint8_t, 12>& iv,
                                                      std::string_view cipher_and_tag,
                                                      std::string_view aad) {
    if (cipher_and_tag.size() < 16) return std::nullopt;
    size_t cipher_len = cipher_and_tag.size() - 16;
    std::array<uint8_t, 16> tag{};
    std::memcpy(tag.data(), cipher_and_tag.data() + cipher_len, 16);

    EvpCipherCtxGuard ctx;
    if (!ctx.get()) return std::nullopt;
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return std::nullopt;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) return std::nullopt;
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()) != 1) return std::nullopt;

    int len = 0;
    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx.get(), nullptr, &len,
                              reinterpret_cast<const unsigned char*>(aad.data()),
                              static_cast<int>(aad.size())) != 1) {
            return std::nullopt;
        }
    }

    std::string out(cipher_len, '\0');
    int plain_len = 0;
    if (cipher_len > 0) {
        if (EVP_DecryptUpdate(ctx.get(),
                              reinterpret_cast<unsigned char*>(out.data()), &len,
                              reinterpret_cast<const unsigned char*>(cipher_and_tag.data()),
                              static_cast<int>(cipher_len)) != 1) {
            return std::nullopt;
        }
        plain_len = len;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag.data()) != 1) {
        return std::nullopt;
    }
    if (EVP_DecryptFinal_ex(ctx.get(),
                            reinterpret_cast<unsigned char*>(out.data()) + plain_len, &len) != 1) {
        return std::nullopt; // Tag mismatch / tampered ciphertext!
    }
    plain_len += len;
    out.resize(static_cast<size_t>(plain_len));
    return out;
}

std::optional<std::string> aes256_gcm_encrypt(const std::array<uint8_t, 32>& key,
                                              std::string_view plaintext,
                                              std::string_view aad) {
    std::array<uint8_t, 12> iv{};
    if (!random_bytes(iv.data(), iv.size())) return std::nullopt;
    auto ct = aes256_gcm_encrypt_with_iv(key, iv, plaintext, aad);
    if (!ct) return std::nullopt;
    std::string combined;
    combined.reserve(12 + ct->size());
    combined.append(reinterpret_cast<const char*>(iv.data()), 12);
    combined.append(*ct);
    return combined;
}

std::optional<std::string> aes256_gcm_decrypt(const std::array<uint8_t, 32>& key,
                                              std::string_view iv_cipher_tag,
                                              std::string_view aad) {
    if (iv_cipher_tag.size() < 28) return std::nullopt;
    std::array<uint8_t, 12> iv{};
    std::memcpy(iv.data(), iv_cipher_tag.data(), 12);
    return aes256_gcm_decrypt_with_iv(key, iv, iv_cipher_tag.substr(12), aad);
}

std::string vault_encrypt(std::string_view master_secret, std::string_view plaintext) {
    if (plaintext.empty() || plaintext.starts_with("enc:v1:")) {
        return std::string(plaintext);
    }
    auto key = derive_key_sha256(master_secret, "alist-core-vault-v1");
    auto enc = aes256_gcm_encrypt(key, plaintext, "alist-vault-aad");
    OPENSSL_cleanse(key.data(), key.size());
    if (!enc) return std::string(plaintext);
    return "enc:v1:" + base64url_encode(*enc);
}

std::string vault_decrypt(std::string_view master_secret, std::string_view stored) {
    if (!stored.starts_with("enc:v1:")) {
        return std::string(stored);
    }
    auto bin = base64url_decode(stored.substr(7));
    if (!bin) return "";
    auto key = derive_key_sha256(master_secret, "alist-core-vault-v1");
    auto dec = aes256_gcm_decrypt(key, *bin, "alist-vault-aad");
    OPENSSL_cleanse(key.data(), key.size());
    return dec.value_or("");
}

std::string jwt_encode_hs256(const JwtClaims& claims, std::string_view secret) {
    static constexpr std::string_view kHeaderJson = R"({"alg":"HS256","typ":"JWT"})";
    JsonValue payload = JsonValue::object({
        {"username", claims.username},
        {"uid",      static_cast<int64_t>(claims.user_id)},
        {"pwd_ts",   claims.pwd_ts},
        {"iat",      claims.iat},
        {"nbf",      claims.nbf},
        {"exp",      claims.exp},
        {"jti",      claims.jti.empty() ? random_hex(12) : claims.jti},
    });
    std::string h_b64 = base64url_encode(kHeaderJson);
    std::string p_b64 = base64url_encode(payload.dump());
    std::string signing_input = h_b64 + "." + p_b64;
    auto mac = hmac_sha256_bytes(secret, signing_input);
    std::string s_b64 = base64url_encode(std::string_view(reinterpret_cast<const char*>(mac.data()), mac.size()));
    return signing_input + "." + s_b64;
}

std::optional<JwtClaims> jwt_decode_hs256(std::string_view token, std::string_view secret, std::string* err_out) {
    if (token.starts_with("Bearer ") || token.starts_with("bearer ")) {
        token.remove_prefix(7);
        token = trim_ascii(token);
    }
    auto dot1 = token.find('.');
    if (dot1 == std::string_view::npos) {
        if (err_out) *err_out = "Malformed JWT";
        return std::nullopt;
    }
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos || token.find('.', dot2 + 1) != std::string_view::npos) {
        if (err_out) *err_out = "Malformed JWT segments";
        return std::nullopt;
    }

    std::string_view h_b64 = token.substr(0, dot1);
    std::string_view p_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string_view s_b64 = token.substr(dot2 + 1);

    // 1. Verify header algorithm strictly BEFORE trusting claims (prevents alg:none & confusion!)
    auto hdr_raw = base64url_decode(h_b64);
    if (!hdr_raw) {
        if (err_out) *err_out = "Invalid JWT header encoding";
        return std::nullopt;
    }
    auto hdr_json = JsonValue::parse(*hdr_raw);
    if (!hdr_json || !hdr_json->is_object() || hdr_json->get("alg").as_sv() != "HS256") {
        if (err_out) *err_out = "Unsupported or forged JWT algorithm";
        return std::nullopt;
    }

    // 2. Constant-time signature verification
    std::string_view signing_input = token.substr(0, dot2);
    auto expected_mac = hmac_sha256_bytes(secret, signing_input);
    auto provided_sig = base64url_decode(s_b64);
    if (!provided_sig || provided_sig->size() != 32 ||
        CRYPTO_memcmp(expected_mac.data(), provided_sig->data(), 32) != 0) {
        if (err_out) *err_out = "Invalid JWT signature";
        return std::nullopt;
    }

    // 3. Parse & validate temporal claims
    auto pay_raw = base64url_decode(p_b64);
    if (!pay_raw) {
        if (err_out) *err_out = "Invalid JWT payload encoding";
        return std::nullopt;
    }
    auto pay_json = JsonValue::parse(*pay_raw);
    if (!pay_json || !pay_json->is_object()) {
        if (err_out) *err_out = "Invalid JWT payload JSON";
        return std::nullopt;
    }

    int64_t now = unix_now();
    JwtClaims c;
    c.username = pay_json->get("username").as_str();
    c.user_id  = static_cast<uint64_t>(pay_json->get("uid").as_i64(0));
    c.pwd_ts   = pay_json->get("pwd_ts").as_i64(0);
    c.iat      = pay_json->get("iat").as_i64(0);
    c.nbf      = pay_json->get("nbf").as_i64(0);
    c.exp      = pay_json->get("exp").as_i64(0);
    c.jti      = pay_json->get("jti").as_str();

    if (c.username.empty()) {
        if (err_out) *err_out = "Missing username claim";
        return std::nullopt;
    }
    if (c.exp <= 0 || now >= c.exp) {
        if (err_out) *err_out = "Token has expired";
        return std::nullopt;
    }
    if (c.nbf > 0 && now + 30 < c.nbf) {
        if (err_out) *err_out = "Token is not yet active";
        return std::nullopt;
    }
    return c;
}

std::string sign_url_payload(std::string_view secret, std::string_view payload, int64_t expire_unix) {
    std::string expire_str = std::to_string(expire_unix);
    std::string msg;
    msg.reserve(payload.size() + 1 + expire_str.size());
    msg.append(payload);
    msg.push_back(':');
    msg.append(expire_str);
    auto mac = hmac_sha256_bytes(secret, msg);
    return base64url_encode(std::string_view(reinterpret_cast<const char*>(mac.data()), mac.size())) + ":" + expire_str;
}

bool verify_url_payload(std::string_view secret, std::string_view payload, std::string_view signature) {
    auto colon = signature.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= signature.size()) {
        return false;
    }
    auto exp_opt = parse_i64(signature.substr(colon + 1));
    if (!exp_opt || *exp_opt < 0) return false;
    if (*exp_opt > 0 && unix_now() > *exp_opt) return false; // Expired!

    std::string expected = sign_url_payload(secret, payload, *exp_opt);
    return constant_time_equals(expected, signature);
}

std::string totp_generate_secret() {
    std::array<uint8_t, 20> raw{};
    random_bytes(raw.data(), raw.size());
    return base32_encode(std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()));
}

std::string totp_code_at(std::string_view base32_secret, int64_t unix_sec) {
    auto key_opt = base32_decode(base32_secret);
    if (!key_opt || key_opt->empty()) return "";
    uint64_t step = static_cast<uint64_t>(unix_sec / 30);
    std::array<uint8_t, 8> counter{};
    for (int i = 7; i >= 0; --i) {
        counter[static_cast<size_t>(i)] = static_cast<uint8_t>(step & 0xFF);
        step >>= 8;
    }
    auto mac = hmac_sha1_bytes(*key_opt, std::string_view(reinterpret_cast<const char*>(counter.data()), 8));
    size_t offset = mac[19] & 0x0F;
    uint32_t bin_code = ((static_cast<uint32_t>(mac[offset]) & 0x7F) << 24) |
                        ((static_cast<uint32_t>(mac[offset + 1]) & 0xFF) << 16) |
                        ((static_cast<uint32_t>(mac[offset + 2]) & 0xFF) << 8) |
                        (static_cast<uint32_t>(mac[offset + 3]) & 0xFF);
    uint32_t otp = bin_code % 1000000;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%06u", otp);
    return std::string(buf);
}

bool totp_verify(std::string_view base32_secret, std::string_view code, int64_t unix_sec, int64_t* matched_step_out) {
    code = trim_ascii(code);
    if (code.size() != 6) return false;
    for (int delta = -1; delta <= 1; ++delta) {
        int64_t t = unix_sec + delta * 30;
        std::string expected = totp_code_at(base32_secret, t);
        if (!expected.empty() && constant_time_equals(expected, code)) {
            if (matched_step_out) *matched_step_out = t / 30;
            return true;
        }
    }
    return false;
}

namespace {
std::array<uint8_t, 32> derive_aws_signing_key(std::string_view secret_key,
                                               std::string_view date_yyyymmdd,
                                               std::string_view region,
                                               std::string_view service) {
    std::string k_secret = "AWS4" + std::string(secret_key);
    auto k_date    = hmac_sha256_bytes(k_secret, date_yyyymmdd);
    auto k_region  = hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(k_date.data()), 32), region);
    auto k_service = hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(k_region.data()), 32), service);
    return hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(k_service.data()), 32), "aws4_request");
}
} // namespace

std::map<std::string, std::string> aws_sigv4_sign_headers(
    const AwsSigV4Params& creds,
    std::string_view method,
    std::string_view host,
    std::string_view canonical_uri,
    std::string_view canonical_query,
    std::string_view payload_hash,
    int64_t unix_sec,
    const std::map<std::string, std::string>& extra_headers) {

    std::string amz_date = format_amz_date(unix_sec);
    std::string date_stamp = amz_date.substr(0, 8);
    std::string region = creds.region.empty() ? "auto" : creds.region;

    std::map<std::string, std::string> canonical_hdrs;
    canonical_hdrs["host"] = std::string(host);
    canonical_hdrs["x-amz-content-sha256"] = std::string(payload_hash);
    canonical_hdrs["x-amz-date"] = amz_date;
    for (const auto& [k, v] : extra_headers) {
        canonical_hdrs[to_lower_ascii(k)] = std::string(trim_ascii(v));
    }

    std::string hdr_lines;
    std::string signed_hdrs;
    for (const auto& [k, v] : canonical_hdrs) {
        hdr_lines += k + ":" + v + "\n";
        if (!signed_hdrs.empty()) signed_hdrs.push_back(';');
        signed_hdrs += k;
    }

    std::string canonical_req = std::string(method) + "\n" +
                                std::string(canonical_uri.empty() ? "/" : canonical_uri) + "\n" +
                                std::string(canonical_query) + "\n" +
                                hdr_lines + "\n" +
                                signed_hdrs + "\n" +
                                std::string(payload_hash);

    std::string scope = date_stamp + "/" + region + "/" + creds.service + "/aws4_request";
    std::string string_to_sign = "AWS4-HMAC-SHA256\n" + amz_date + "\n" + scope + "\n" + sha256_hex(canonical_req);

    auto sig_key = derive_aws_signing_key(creds.secret_key, date_stamp, region, creds.service);
    std::string sig_hex = hmac_sha256_hex(std::string_view(reinterpret_cast<const char*>(sig_key.data()), 32), string_to_sign);

    std::map<std::string, std::string> out_headers = extra_headers;
    out_headers["x-amz-date"] = amz_date;
    out_headers["x-amz-content-sha256"] = std::string(payload_hash);
    out_headers["Authorization"] = "AWS4-HMAC-SHA256 Credential=" + creds.access_key + "/" + scope +
                                   ", SignedHeaders=" + signed_hdrs + ", Signature=" + sig_hex;
    return out_headers;
}

std::string aws_sigv4_presign_url(
    const AwsSigV4Params& creds,
    std::string_view method,
    std::string_view scheme,
    std::string_view host,
    std::string_view canonical_uri,
    int64_t unix_sec,
    int64_t expires_seconds) {

    std::string amz_date = format_amz_date(unix_sec);
    std::string date_stamp = amz_date.substr(0, 8);
    std::string region = creds.region.empty() ? "auto" : creds.region;
    std::string scope = date_stamp + "/" + region + "/" + creds.service + "/aws4_request";

    std::map<std::string, std::string> qparams = {
        {"X-Amz-Algorithm",     "AWS4-HMAC-SHA256"},
        {"X-Amz-Credential",    creds.access_key + "/" + scope},
        {"X-Amz-Date",          amz_date},
        {"X-Amz-Expires",       std::to_string(std::clamp<int64_t>(expires_seconds, 1, 604800))},
        {"X-Amz-SignedHeaders", "host"},
    };

    std::string canonical_query;
    for (const auto& [k, v] : qparams) {
        if (!canonical_query.empty()) canonical_query.push_back('&');
        canonical_query += url_encode(k) + "=" + url_encode(v);
    }

    std::string uri = canonical_uri.empty() ? "/" : std::string(canonical_uri);
    std::string canonical_req = std::string(method) + "\n" +
                                uri + "\n" +
                                canonical_query + "\n" +
                                "host:" + std::string(host) + "\n\n" +
                                "host\nUNSIGNED-PAYLOAD";

    std::string string_to_sign = "AWS4-HMAC-SHA256\n" + amz_date + "\n" + scope + "\n" + sha256_hex(canonical_req);
    auto sig_key = derive_aws_signing_key(creds.secret_key, date_stamp, region, creds.service);
    std::string sig_hex = hmac_sha256_hex(std::string_view(reinterpret_cast<const char*>(sig_key.data()), 32), string_to_sign);

    return std::string(scheme) + "://" + std::string(host) + uri + "?" + canonical_query + "&X-Amz-Signature=" + sig_hex;
}

std::optional<std::string> gzip_compress(std::string_view input, int level) {
    z_stream zs{};
    if (alist_deflateInit2(&zs, level, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return std::nullopt;
    }
    ScopeExit guard([&] { deflateEnd(&zs); });

    // zlib's public headers may declare next_in mutable unless ZLIB_CONST is
    // set, although deflate only reads input. The shim declares it const.
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    std::string out;
    out.reserve(input.size() / 2 + 64);
    std::array<uint8_t, 16384> chunk{};
    int ret = Z_OK;
    do {
        zs.next_out = chunk.data();
        zs.avail_out = static_cast<uInt>(chunk.size());
        ret = deflate(&zs, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            return std::nullopt;
        }
        size_t produced = chunk.size() - zs.avail_out;
        out.append(reinterpret_cast<const char*>(chunk.data()), produced);
    } while (ret != Z_STREAM_END);

    return out;
}

std::optional<std::string> raw_inflate(std::string_view compressed, size_t max_output_bytes) {
    z_stream zs{};
    if (alist_inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }
    ScopeExit guard([&] { inflateEnd(&zs); });

    // inflate reads but does not modify the supplied input buffer.
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
    zs.avail_in = static_cast<uInt>(compressed.size());

    std::string out;
    std::array<uint8_t, 16384> chunk{};
    int ret = Z_OK;
    do {
        zs.next_out = chunk.data();
        zs.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return std::nullopt;
        }
        size_t produced = chunk.size() - zs.avail_out;
        if (out.size() + produced > max_output_bytes) {
            return std::nullopt; // Zip-Bomb protection!
        }
        out.append(reinterpret_cast<const char*>(chunk.data()), produced);
    } while (ret != Z_STREAM_END && zs.avail_in > 0);

    if (ret != Z_STREAM_END) return std::nullopt;
    return out;
}

uint32_t crc32_compute(std::string_view data, uint32_t init_crc) {
    uLong c = (init_crc == 0) ? crc32(0L, nullptr, 0) : static_cast<uLong>(init_crc);
    return static_cast<uint32_t>(
        crc32(c, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size())));
}

} // namespace alist
