// Cross-language fixture helper. Test-only: never use these fixed credentials.
#include "alist/crypto.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    const std::string secret = "test-vault-secret";
    const std::string plaintext = "Kova compatible: \xe6\x96\x87\xe4\xbb\xb6";
    if (mode == "encrypt") {
        std::cout << alist::vault_encrypt(secret, plaintext);
    } else if (mode == "decrypt") {
        std::string value;
        if (!std::getline(std::cin, value)) return 2;
        std::cout << alist::vault_decrypt(secret, value);
    } else if (mode == "sign") {
        std::cout << alist::sign_url_payload(secret, "/local/a b.txt", 2000000000);
    } else if (mode == "password-hash") {
        std::cout << alist::hash_password_pbkdf2("test-password-\xe6\x96\x87", 10000);
    } else if (mode == "password-verify") {
        std::string value;
        if (!std::getline(std::cin, value)) return 2;
        if (!alist::verify_password_pbkdf2("test-password-\xe6\x96\x87", value)) return 1;
        std::cout << "ok";
    } else if (mode == "totp") {
        std::cout << alist::totp_code_at("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 1234567890);
    } else if (mode == "jwt-encode") {
        alist::JwtClaims claims;
        claims.username = "alice";
        claims.user_id = 42;
        claims.pwd_ts = 1234;
        claims.iat = 1700000000;
        claims.nbf = 1700000000;
        claims.exp = 4102444800;
        claims.jti = "fixture-id";
        std::cout << alist::jwt_encode_hs256(claims, secret);
    } else if (mode == "jwt-decode") {
        std::string value;
        if (!std::getline(std::cin, value)) return 2;
        auto claims = alist::jwt_decode_hs256(value, secret);
        if (!claims || claims->username != "alice" || claims->user_id != 42 ||
            claims->pwd_ts != 1234 || claims->iat != 1700000000 ||
            claims->nbf != 1700000000 || claims->exp != 4102444800 || claims->jti != "fixture-id") return 1;
        std::cout << "ok";
    } else {
        return 2;
    }
}
