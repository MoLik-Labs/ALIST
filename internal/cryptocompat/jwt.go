package cryptocompat

import (
	"crypto/hmac"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"strings"
)

// Claims matches the native JWT claim names. Session revocation, password
// timestamp checks and user lookup remain the responsibility of the caller.
type Claims struct {
	Username          string `json:"username"`
	UserID            int64  `json:"uid"`
	PasswordTimestamp int64  `json:"pwd_ts"`
	IssuedAt          int64  `json:"iat"`
	NotBefore         int64  `json:"nbf"`
	Expires           int64  `json:"exp"`
	ID                string `json:"jti"`
}

var ErrInvalidJWT = errors.New("invalid JWT")

const jwtHeader = `{"alg":"HS256","typ":"JWT"}`

// EncodeJWT generates a native-compatible HS256 JWT. A random 12-byte ID is
// generated if absent. Callers must supply a securely generated secret.
func EncodeJWT(claims Claims, secret string) (string, error) {
	if claims.ID == "" {
		var id [12]byte
		if _, err := rand.Read(id[:]); err != nil {
			return "", err
		}
		claims.ID = hex.EncodeToString(id[:])
	}
	payload, err := json.Marshal(claims)
	if err != nil {
		return "", err
	}
	header := base64.RawURLEncoding.EncodeToString([]byte(jwtHeader))
	body := base64.RawURLEncoding.EncodeToString(payload)
	message := header + "." + body
	return message + "." + base64.RawURLEncoding.EncodeToString(mac256([]byte(secret), []byte(message))), nil
}

// DecodeJWT pins HS256, verifies the MAC and checks username and expiry. Like
// the native implementation, nbf has a 30-second allowance. For compatibility,
// a zero nbf imposes no constraint. Authentication is not complete until callers
// also validate user status, password timestamp and session revocation.
func DecodeJWT(token, secret string, now int64) (Claims, error) {
	var claims Claims
	if strings.HasPrefix(token, "Bearer ") || strings.HasPrefix(token, "bearer ") {
		token = strings.Trim(token[7:], " \t\r\n")
	}
	// Bound work before decoding attacker-controlled data.
	if len(token) > 16384 {
		return claims, ErrInvalidJWT
	}
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		return claims, ErrInvalidJWT
	}
	decode := base64.RawURLEncoding.Strict().DecodeString
	headerBytes, err := decode(parts[0])
	if err != nil {
		return claims, ErrInvalidJWT
	}
	var header struct {
		Algorithm string `json:"alg"`
	}
	if json.Unmarshal(headerBytes, &header) != nil || header.Algorithm != "HS256" {
		return claims, ErrInvalidJWT
	}
	signature, err := decode(parts[2])
	if err != nil || !hmac.Equal(signature, mac256([]byte(secret), []byte(parts[0]+"."+parts[1]))) {
		return claims, ErrInvalidJWT
	}
	payload, err := decode(parts[1])
	if err != nil || json.Unmarshal(payload, &claims) != nil {
		return Claims{}, ErrInvalidJWT
	}
	// Subtract from the positive nbf rather than adding to now to avoid overflow.
	if claims.Username == "" || claims.Expires <= 0 || now >= claims.Expires || (claims.NotBefore > 0 && claims.NotBefore-30 > now) {
		return Claims{}, ErrInvalidJWT
	}
	return claims, nil
}
