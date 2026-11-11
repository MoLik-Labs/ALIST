#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alist {

struct JwtClaims {
    std::string username;
    uint64_t user_id = 0;
    int64_t pwd_ts   = 0;
    int64_t iat      = 0;
    int64_t nbf      = 0;
    int64_t exp      = 0;
    std::string jti;
};

struct AwsSigV4Params {
    std::string access_key;
    std::string secret_key;
    std::string region;
    std::string service = "s3";
};

// CSPRNG & Constant-time primitives
bool random_bytes(uint8_t* out, size_t len);
std::string random_hex(size_t num_bytes);
std::string random_base64url(size_t num_bytes);
bool constant_time_equals(std::string_view a, std::string_view b) noexcept;

// Hashing & MAC
std::array<uint8_t, 32> sha256_bytes(std::string_view data);
std::string sha256_hex(std::string_view data);
std::string md5_hex(std::string_view data);
std::array<uint8_t, 32> hmac_sha256_bytes(std::string_view key, std::string_view data);
std::string hmac_sha256_hex(std::string_view key, std::string_view data);
std::array<uint8_t, 20> hmac_sha1_bytes(std::string_view key, std::string_view data);
std::string hex_encode(const uint8_t* data, size_t len);

// Encodings
std::string base64_encode(std::string_view bin);
std::optional<std::string> base64_decode(std::string_view b64);
std::string base64url_encode(std::string_view bin);
std::optional<std::string> base64url_decode(std::string_view b64u);
std::string base32_encode(std::string_view bin);
std::optional<std::string> base32_decode(std::string_view b32);

// Password Hashing (PBKDF2-HMAC-SHA256, 210,000 iterations, CSPRNG 16B salt)
std::string hash_password_pbkdf2(std::string_view password, int iterations = 210000);
bool verify_password_pbkdf2(std::string_view password, std::string_view stored_hash);

// AES-256-GCM Authenticated Encryption & Master Vault
std::array<uint8_t, 32> derive_key_sha256(std::string_view master_secret, std::string_view context);
std::optional<std::string> aes256_gcm_encrypt(const std::array<uint8_t, 32>& key,
                                              std::string_view plaintext,
                                              std::string_view aad = "");
std::optional<std::string> aes256_gcm_decrypt(const std::array<uint8_t, 32>& key,
                                              std::string_view iv_cipher_tag,
                                              std::string_view aad = "");
// Deterministic-IV chunk AEAD for random-access HTTP Range seeking in Crypt driver
std::optional<std::string> aes256_gcm_encrypt_with_iv(const std::array<uint8_t, 32>& key,
                                                      const std::array<uint8_t, 12>& iv,
                                                      std::string_view plaintext,
                                                      std::string_view aad = "");
std::optional<std::string> aes256_gcm_decrypt_with_iv(const std::array<uint8_t, 32>& key,
                                                      const std::array<uint8_t, 12>& iv,
                                                      std::string_view cipher_and_tag,
                                                      std::string_view aad = "");

std::string vault_encrypt(std::string_view master_secret, std::string_view plaintext);
std::string vault_decrypt(std::string_view master_secret, std::string_view stored);

// JWT (HS256 strictly pinned)
std::string jwt_encode_hs256(const JwtClaims& claims, std::string_view secret);
std::optional<JwtClaims> jwt_decode_hs256(std::string_view token, std::string_view secret, std::string* err_out = nullptr);

// Signed Download & Share URLs
std::string sign_url_payload(std::string_view secret, std::string_view payload, int64_t expire_unix);
bool verify_url_payload(std::string_view secret, std::string_view payload, std::string_view signature);

// RFC 6238 TOTP 2FA
std::string totp_generate_secret();
std::string totp_code_at(std::string_view base32_secret, int64_t unix_sec);
bool totp_verify(std::string_view base32_secret, std::string_view code, int64_t unix_sec, int64_t* matched_step_out = nullptr);

// AWS Signature Version 4 (for S3 / R2 / MinIO / B2)
std::map<std::string, std::string> aws_sigv4_sign_headers(
    const AwsSigV4Params& creds,
    std::string_view method,
    std::string_view host,
    std::string_view canonical_uri,
    std::string_view canonical_query,
    std::string_view payload_hash,
    int64_t unix_sec,
    const std::map<std::string, std::string>& extra_headers = {});

std::string aws_sigv4_presign_url(
    const AwsSigV4Params& creds,
    std::string_view method,
    std::string_view scheme,
    std::string_view host,
    std::string_view canonical_uri,
    int64_t unix_sec,
    int64_t expires_seconds = 3600);

// Compression & Checksums (zlib)
std::optional<std::string> gzip_compress(std::string_view input, int level = 6);
std::optional<std::string> raw_inflate(std::string_view compressed, size_t max_output_bytes);
uint32_t crc32_compute(std::string_view data, uint32_t init_crc = 0);

} // namespace alist
