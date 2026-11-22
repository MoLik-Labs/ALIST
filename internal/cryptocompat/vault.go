// Package cryptocompat implements selected Kova native wire/storage formats.
// It is migration infrastructure, not a complete authentication system.
package cryptocompat

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"strconv"
	"strings"
)

const vaultPrefix = "enc:v1:"
const vaultContext = "alist-core-vault-v1"
const vaultAAD = "alist-vault-aad"

var ErrInvalidVault = errors.New("invalid or unauthenticated vault value")

func mac256(key, message []byte) []byte {
	mac := hmac.New(sha256.New, key)
	_, _ = mac.Write(message)
	return mac.Sum(nil)
}

// EncryptVault writes the native enc:v1 format: base64url(nonce || ciphertext
// || GCM tag). Encryption errors are returned, never silently stored as plaintext.
// Already-encoded values and empty strings are left unchanged, like native Kova.
func EncryptVault(secret, plaintext string) (string, error) {
	if plaintext == "" || strings.HasPrefix(plaintext, vaultPrefix) {
		return plaintext, nil
	}
	aead, err := vaultCipher(secret)
	if err != nil {
		return "", err
	}
	nonce := make([]byte, aead.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return "", fmt.Errorf("vault nonce: %w", err)
	}
	blob := aead.Seal(nonce, nonce, []byte(plaintext), []byte(vaultAAD))
	return vaultPrefix + base64.RawURLEncoding.EncodeToString(blob), nil
}

// DecryptVault passes legacy plaintext through. An invalid encrypted value is
// reported as an error, rather than being confused with a legitimate empty value.
func DecryptVault(secret, stored string) (string, error) {
	if !strings.HasPrefix(stored, vaultPrefix) {
		return stored, nil
	}
	blob, err := base64.RawURLEncoding.Strict().DecodeString(strings.TrimPrefix(stored, vaultPrefix))
	if err != nil {
		return "", ErrInvalidVault
	}
	aead, err := vaultCipher(secret)
	if err != nil {
		return "", err
	}
	if len(blob) < aead.NonceSize()+aead.Overhead() {
		return "", ErrInvalidVault
	}
	plaintext, err := aead.Open(nil, blob[:aead.NonceSize()], blob[aead.NonceSize():], []byte(vaultAAD))
	if err != nil {
		return "", ErrInvalidVault
	}
	return string(plaintext), nil
}

func vaultCipher(secret string) (cipher.AEAD, error) {
	key := mac256([]byte(secret), []byte(vaultContext))
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	return cipher.NewGCM(block)
}

// SignURL returns the native base64url(HMAC-SHA256(payload:expiry)):expiry format.
// An expiry of zero means no expiry; callers should prefer bounded lifetimes.
func SignURL(secret, payload string, expiry int64) string {
	exp := strconv.FormatInt(expiry, 10)
	sum := mac256([]byte(secret), []byte(payload+":"+exp))
	return base64.RawURLEncoding.EncodeToString(sum) + ":" + exp
}

// VerifyURL checks the exact canonical signature and its expiry. now is Unix
// seconds, supplied by the caller so boundary behavior can be tested.
func VerifyURL(secret, payload, signature string, now int64) bool {
	colon := strings.LastIndexByte(signature, ':')
	if colon <= 0 || colon == len(signature)-1 {
		return false
	}
	exp, err := strconv.ParseInt(signature[colon+1:], 10, 64)
	if err != nil || exp < 0 || (exp > 0 && now > exp) {
		return false
	}
	return hmac.Equal([]byte(SignURL(secret, payload, exp)), []byte(signature))
}
