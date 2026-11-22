package cryptocompat

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"errors"
	"strconv"
	"strings"

	"github.com/MoLik-Labs/ALIST/internal/thirdparty/pbkdf2"
)

const PasswordIterations = 210000
const maxPasswordIterations = 5000000

var ErrPasswordParameters = errors.New("invalid password hashing parameters")

// HashPassword produces the native pbkdf2$sha256 format with a random 16-byte
// salt and the native default iteration count. Failure to obtain randomness is
// returned to the caller; a hash is never emitted with a predictable salt.
func HashPassword(password string) (string, error) {
	var salt [16]byte
	if _, err := rand.Read(salt[:]); err != nil {
		return "", err
	}
	return hashPassword(password, salt[:], PasswordIterations)
}

func hashPassword(password string, salt []byte, iterations int) (string, error) {
	if iterations < 1000 || iterations > maxPasswordIterations || len(salt) > 1024 {
		return "", ErrPasswordParameters
	}
	dk := pbkdf2.Key([]byte(password), salt, iterations, 32, sha256.New)
	return "pbkdf2$sha256$" + strconv.Itoa(iterations) + "$" + base64.RawURLEncoding.EncodeToString(salt) + "$" + base64.RawURLEncoding.EncodeToString(dk), nil
}

// VerifyPassword verifies native hashes using constant-time key comparison.
// Work and input sizes are bounded before key derivation. Authentication,
// rate-limiting and concurrent work limits must be enforced by the caller.
func VerifyPassword(password, stored string) bool {
	if len(stored) > 4096 {
		return false
	}
	parts := strings.Split(stored, "$")
	if len(parts) != 5 || parts[0] != "pbkdf2" || parts[1] != "sha256" {
		return false
	}
	iterations, err := strconv.Atoi(parts[2])
	if err != nil || iterations < 1000 || iterations > maxPasswordIterations {
		return false
	}
	decode := base64.RawURLEncoding.Strict().DecodeString
	salt, err := decode(parts[3])
	if err != nil || len(salt) > 1024 {
		return false
	}
	expected, err := decode(parts[4])
	if err != nil || len(expected) != 32 {
		return false
	}
	actual := pbkdf2.Key([]byte(password), salt, iterations, 32, sha256.New)
	return hmac.Equal(expected, actual)
}
