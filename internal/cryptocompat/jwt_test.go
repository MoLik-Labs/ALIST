package cryptocompat

import (
	"encoding/base64"
	"os"
	"os/exec"
	"strings"
	"testing"
)

func fixtureClaims() Claims {
	return Claims{Username: "alice", UserID: 42, PasswordTimestamp: 1234, IssuedAt: 1700000000, NotBefore: 1700000000, Expires: 4102444800, ID: "fixture-id"}
}

func TestJWT(t *testing.T) {
	claims := fixtureClaims()
	token, err := EncodeJWT(claims, testSecret)
	if err != nil {
		t.Fatal(err)
	}
	decoded, err := DecodeJWT("Bearer "+token, testSecret, 1700000000)
	if err != nil || decoded != claims {
		t.Fatal(decoded, err)
	}
	for _, bad := range []string{"", token + ".x", token + "x", strings.Repeat("x", 16385)} {
		if _, err := DecodeJWT(bad, testSecret, 1700000000); err == nil {
			t.Fatalf("accepted %q", bad[:min(len(bad), 100)])
		}
	}
	if _, err := DecodeJWT(token, "wrong", 1700000000); err == nil {
		t.Fatal("wrong key accepted")
	}
	if _, err := DecodeJWT(token, testSecret, claims.Expires); err == nil {
		t.Fatal("expired accepted")
	}
	if _, err := DecodeJWT(token, testSecret, claims.NotBefore-31); err == nil {
		t.Fatal("early token accepted")
	}
	if _, err := DecodeJWT(token, testSecret, claims.NotBefore-30); err != nil {
		t.Fatal("leeway rejected", err)
	}
	parts := strings.Split(token, ".")
	parts[0] = base64.RawURLEncoding.EncodeToString([]byte(`{"alg":"none"}`))
	if _, err := DecodeJWT(strings.Join(parts, "."), testSecret, 1700000000); err == nil {
		t.Fatal("none algorithm accepted")
	}
	claims.ID = ""
	a, err := EncodeJWT(claims, testSecret)
	if err != nil {
		t.Fatal(err)
	}
	b, err := EncodeJWT(claims, testSecret)
	if err != nil || a == b {
		t.Fatal("random jti failure")
	}
}

func TestNativeJWT(t *testing.T) {
	binary := os.Getenv("KOVA_NATIVE_CRYPTO_FIXTURE")
	if binary == "" {
		t.Skip("set KOVA_NATIVE_CRYPTO_FIXTURE for native interoperability")
	}
	out, err := exec.Command(binary, "jwt-encode").Output()
	if err != nil {
		t.Fatal(err)
	}
	claims, err := DecodeJWT(string(out), testSecret, 1800000000)
	if err != nil || claims != fixtureClaims() {
		t.Fatal(claims, err)
	}
	token, err := EncodeJWT(fixtureClaims(), testSecret)
	if err != nil {
		t.Fatal(err)
	}
	cmd := exec.Command(binary, "jwt-decode")
	cmd.Stdin = strings.NewReader(token + "\n")
	out, err = cmd.Output()
	if err != nil || string(out) != "ok" {
		t.Fatal(string(out), err)
	}
}

func FuzzDecodeJWT(f *testing.F) {
	f.Add("")
	f.Add("a.b.c")
	f.Fuzz(func(t *testing.T, s string) { _, _ = DecodeJWT(s, testSecret, 1800000000) })
}
