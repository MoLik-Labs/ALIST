package s3store

import (
	"context"
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
	"time"
)

func TestAWSOfficialPresignedGetVector(t *testing.T) {
	c, err := New(Config{BucketURL: "https://examplebucket.s3.amazonaws.com", Region: "us-east-1", AccessKey: "AKIAIOSFODNN7EXAMPLE", SecretKey: "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	got, err := c.presignGet("test.txt", time.Date(2013, 5, 24, 0, 0, 0, 0, time.UTC), 86400)
	if err != nil {
		t.Fatal(err)
	}
	want := "https://examplebucket.s3.amazonaws.com/test.txt?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20130524%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20130524T000000Z&X-Amz-Expires=86400&X-Amz-SignedHeaders=host&X-Amz-Signature=aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404"
	if got != want {
		t.Fatalf("signature mismatch\ngot:  %s\nwant: %s", got, want)
	}
}

func TestConfigAndKeysRejectUnsafeInput(t *testing.T) {
	good := Config{BucketURL: "https://example.com/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret"}
	for _, endpoint := range []string{"http://example.com", "https://user:pass@example.com/", "https://example.com/?query=x", "https://example.com/a/../b", "https://example.com/%2fsecret", "https://example.com/#fragment"} {
		c := good
		c.BucketURL = endpoint
		if _, err := New(c); err == nil {
			t.Errorf("unsafe endpoint %q", endpoint)
		}
	}
	c, err := New(good)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	for _, key := range []string{"", "/file", "a/../b", "a/./b", "a//b", "a\n", strings.Repeat("x", 1025)} {
		if _, err := c.presignGet(key, time.Now(), 60); err == nil {
			t.Errorf("unsafe key %q", key)
		}
	}
	if _, err := c.presignGet("file", time.Now(), 0); err == nil {
		t.Fatal("zero expiry")
	}
}

func TestStreamingReadAndNoRedirect(t *testing.T) {
	seen := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		seen++
		if r.URL.Path == "/bucket/redirect" {
			http.Redirect(w, r, "http://127.0.0.1/secret", 302)
			return
		}
		if r.URL.Path != "/bucket/a b" || r.URL.Query().Get("X-Amz-Signature") == "" {
			t.Errorf("request: %s", r.URL.String())
			http.Error(w, "bad", 400)
			return
		}
		w.Write([]byte("hello"))
	}))
	defer server.Close()
	c, err := New(Config{BucketURL: server.URL + "/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret", AllowPrivate: true, AllowInsecureHTTP: true})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	r, err := c.Open(context.Background(), "a b")
	if err != nil {
		t.Fatal(err)
	}
	b, err := io.ReadAll(r)
	r.Close()
	if err != nil || string(b) != "hello" {
		t.Fatal(string(b), err)
	}
	if _, err := c.Open(context.Background(), "redirect"); err == nil || strings.Contains(err.Error(), "X-Amz-") {
		t.Fatal("redirect or secret leak", err)
	}
	if seen != 2 {
		t.Fatal("redirect followed", seen)
	}
}

func TestPresignEncodingAndSessionToken(t *testing.T) {
	c, err := New(Config{BucketURL: "https://s3.example.com/bucket", Region: "auto", AccessKey: "ACCESS", SecretKey: "SECRET", SessionToken: "a+/="})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	signed, err := c.presignGet("folder/đặc biệt+%.txt", time.Date(2026, 1, 2, 0, 0, 0, 0, time.UTC), 60)
	if err != nil {
		t.Fatal(err)
	}
	for _, part := range []string{"/bucket/folder/%C4%91%E1%BA%B7c%20bi%E1%BB%87t%2B%25.txt", "X-Amz-Security-Token=a%2B%2F%3D", "X-Amz-Expires=60"} {
		if !strings.Contains(signed, part) {
			t.Errorf("missing %s", part)
		}
	}
	if strings.Contains(signed, "SECRET") {
		t.Fatal("secret leaked")
	}
}

func TestPrivateEndpointIsDeniedByDefault(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { t.Error("unexpected request to private address") }))
	defer server.Close()
	c, err := New(Config{BucketURL: server.URL, Region: "auto", AccessKey: "key", SecretKey: "secret", AllowInsecureHTTP: true})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	if _, err := c.Open(context.Background(), "object"); err == nil || strings.Contains(err.Error(), "X-Amz-") || strings.Contains(err.Error(), "secret") {
		t.Fatal("private access or credential leak", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := c.Open(ctx, "object"); !errors.Is(err, context.Canceled) {
		t.Fatal("cancellation", err)
	}
}

func FuzzPresignObjectKey(f *testing.F) {
	for _, key := range []string{"folder/file.txt", "../etc/passwd", "hello world", "файл", "x?y#z"} {
		f.Add(key)
	}
	c, err := New(Config{BucketURL: "https://example.com/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret"})
	if err != nil {
		f.Fatal(err)
	}
	defer c.Close()
	f.Fuzz(func(t *testing.T, key string) {
		signed, err := c.presignGet(key, time.Date(2026, 1, 1, 0, 0, 0, 0, time.UTC), 60)
		if err == nil {
			u, e := url.Parse(signed)
			if e != nil || u.Host != "example.com" || u.Scheme != "https" || u.Query().Get("X-Amz-Signature") == "" || strings.Contains(signed, "secret") {
				t.Fatalf("unsafe URL %q: %v", signed, e)
			}
		}
	})
}

func BenchmarkPresignGet(b *testing.B) {
	c, err := New(Config{BucketURL: "https://example.com/bucket", Region: "us-east-1", AccessKey: "access", SecretKey: "secret"})
	if err != nil {
		b.Fatal(err)
	}
	defer c.Close()
	at := time.Date(2026, 1, 1, 0, 0, 0, 0, time.UTC)
	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err := c.presignGet("folder/example file.txt", at, 60); err != nil {
			b.Fatal(err)
		}
	}
}
