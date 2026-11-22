package cryptocompat

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha1" // RFC 6238 / native Kova interoperability.
	"encoding/base32"
	"encoding/binary"
	"errors"
	"fmt"
	"strings"
)

var ErrInvalidTOTPSecret = errors.New("invalid TOTP secret")

// GenerateTOTPSecret creates a native-compatible 20-byte Base32 secret.
func GenerateTOTPSecret() (string, error) {
	var raw [20]byte
	if _, err := rand.Read(raw[:]); err != nil {
		return "", err
	}
	return base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString(raw[:]), nil
}

func decodeTOTPSecret(secret string) ([]byte, error) {
	if len(secret) > 1024 {
		return nil, ErrInvalidTOTPSecret
	}
	// The native decoder allows spaces, padding characters, and ASCII lowercase.
	var cleaned strings.Builder
	cleaned.Grow(len(secret))
	for i := 0; i < len(secret); i++ {
		c := secret[i]
		if c == ' ' || c == '=' {
			continue
		}
		if c >= 'a' && c <= 'z' {
			c -= 'a' - 'A'
		}
		if !((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7')) {
			return nil, ErrInvalidTOTPSecret
		}
		cleaned.WriteByte(c)
	}
	key, err := base32.StdEncoding.WithPadding(base32.NoPadding).DecodeString(cleaned.String())
	if err != nil || len(key) == 0 {
		return nil, ErrInvalidTOTPSecret
	}
	return key, nil
}

func totpForStep(key []byte, step int64) string {
	var counter [8]byte
	binary.BigEndian.PutUint64(counter[:], uint64(step))
	mac := hmac.New(sha1.New, key)
	_, _ = mac.Write(counter[:])
	digest := mac.Sum(nil)
	offset := int(digest[len(digest)-1] & 15)
	value := binary.BigEndian.Uint32(digest[offset:offset+4]) & 0x7fffffff
	return fmt.Sprintf("%06d", value%1000000)
}

// TOTPCode returns the six-digit RFC 6238 SHA-1 code at a nonnegative Unix time.
func TOTPCode(secret string, unixSeconds int64) (string, error) {
	if unixSeconds < 0 {
		return "", ErrInvalidTOTPSecret
	}
	key, err := decodeTOTPSecret(secret)
	if err != nil {
		return "", err
	}
	return totpForStep(key, unixSeconds/30), nil
}

// VerifyTOTP accepts one step of clock drift on either side and returns the
// matched step. The caller must persist and atomically enforce replay protection.
func VerifyTOTP(secret, code string, unixSeconds int64) (int64, bool) {
	code = strings.Trim(code, " \t\r\n")
	if len(code) != 6 || unixSeconds < 0 {
		return 0, false
	}
	key, err := decodeTOTPSecret(secret)
	if err != nil {
		return 0, false
	}
	step := unixSeconds / 30
	for delta := int64(-1); delta <= 1; delta++ {
		if step+delta < 0 {
			continue
		}
		candidate := totpForStep(key, step+delta)
		if hmac.Equal([]byte(candidate), []byte(code)) {
			return step + delta, true
		}
	}
	return 0, false
}
