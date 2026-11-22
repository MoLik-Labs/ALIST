package cryptocompat

import (
	"os"
	"os/exec"
	"strings"
	"testing"
)

func TestPassword(t *testing.T) {
	hash, err := HashPassword("a strong password")
	if err != nil {
		t.Fatal(err)
	}
	if !VerifyPassword("a strong password", hash) || VerifyPassword("wrong", hash) {
		t.Fatal("password verification failed")
	}
	other, err := HashPassword("a strong password")
	if err != nil || hash == other {
		t.Fatal("salt was not random", err)
	}
	for _, bad := range []string{"", "pbkdf2$sha256$999$x$x", "pbkdf2$sha256$5000001$x$x", "pbkdf2$sha256$1000$!$!", strings.Repeat("x", 4097)} {
		if VerifyPassword("password", bad) {
			t.Fatalf("accepted bad hash %q", bad[:min(len(bad), 80)])
		}
	}
}

func TestTOTP(t *testing.T) {
	// RFC 6238 Appendix B SHA-1 vectors, truncated to six digits.
	const secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"
	for _, tc := range []struct {
		unix int64
		want string
	}{{59, "287082"}, {1111111109, "081804"}, {1111111111, "050471"}, {1234567890, "005924"}, {2000000000, "279037"}, {20000000000, "353130"}} {
		code, err := TOTPCode(secret, tc.unix)
		if err != nil || code != tc.want {
			t.Errorf("TOTP %d = %q %v, want %s", tc.unix, code, err, tc.want)
		}
		step, ok := VerifyTOTP(strings.ToLower(secret), " "+code+"\t", tc.unix+30)
		if !ok || step != tc.unix/30 {
			t.Errorf("verification window: %d %v", step, ok)
		}
	}
	if _, ok := VerifyTOTP(secret, "bad", 59); ok {
		t.Fatal("bad code accepted")
	}
	if _, ok := VerifyTOTP(secret, "287082", 120); ok {
		t.Fatal("outside window accepted")
	}
	for _, bad := range []string{"", "!", "ſ", strings.Repeat("A", 1025)} {
		if _, err := TOTPCode(bad, 59); err == nil {
			t.Fatalf("bad secret accepted %q", bad)
		}
	}
	generated, err := GenerateTOTPSecret()
	if err != nil || len(generated) != 32 {
		t.Fatal(generated, err)
	}
	if _, err := TOTPCode(generated, 59); err != nil {
		t.Fatal(err)
	}
}

func TestNativeCredentials(t *testing.T) {
	binary := os.Getenv("KOVA_NATIVE_CRYPTO_FIXTURE")
	if binary == "" {
		t.Skip("set KOVA_NATIVE_CRYPTO_FIXTURE for native interoperability")
	}
	run := func(mode, input string) string {
		t.Helper()
		cmd := exec.Command(binary, mode)
		cmd.Stdin = strings.NewReader(input)
		out, err := cmd.Output()
		if err != nil {
			t.Fatalf("native %s: %v", mode, err)
		}
		return string(out)
	}
	cppHash := run("password-hash", "")
	if !VerifyPassword("test-password-文", cppHash) || VerifyPassword("wrong", cppHash) {
		t.Fatal("could not verify native password")
	}
	goHash, err := hashPassword("test-password-文", []byte("test salt 123456"), 10000)
	if err != nil || run("password-verify", goHash+"\n") != "ok" {
		t.Fatal("native password verification failed", err)
	}
	code, err := TOTPCode("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 1234567890)
	if err != nil || run("totp", "") != code {
		t.Fatal("native TOTP differs", err)
	}
}
