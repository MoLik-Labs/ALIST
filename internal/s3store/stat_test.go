package s3store

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
	"time"
)

func TestStat(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodHead || r.URL.Query().Get("X-Amz-Signature") == "" {
			t.Error("expected signed HEAD")
			w.WriteHeader(400)
			return
		}
		switch r.URL.Path {
		case "/bucket/missing":
			w.WriteHeader(404)
		case "/bucket/denied":
			w.WriteHeader(403)
		case "/bucket/bad-date":
			w.Header().Set("Last-Modified", "bad")
			w.Header().Set("Content-Length", "0")
		default:
			w.Header().Set("Content-Length", "123")
			w.Header().Set("ETag", `"opaque-2"`)
			w.Header().Set("Content-Type", "text/plain")
			w.Header().Set("Last-Modified", "Wed, 21 Oct 2015 07:28:00 GMT")
		}
	}))
	defer server.Close()
	c, err := New(Config{BucketURL: server.URL + "/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret", AllowPrivate: true, AllowInsecureHTTP: true})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	info, err := c.Stat(context.Background(), "file")
	if err != nil || info.Size != 123 || info.ETag != `"opaque-2"` || info.ContentType != "text/plain" || info.LastModified.Format(http.TimeFormat) != "Wed, 21 Oct 2015 07:28:00 GMT" {
		t.Fatalf("%+v %v", info, err)
	}
	_, err = c.Stat(context.Background(), "missing")
	if !errors.Is(err, ErrNotFound) {
		t.Fatal(err)
	}
	_, err = c.Stat(context.Background(), "denied")
	if !errors.Is(err, ErrRequestFailed) || strings.Contains(err.Error(), "X-Amz") {
		t.Fatal(err)
	}
	_, err = c.Stat(context.Background(), "bad-date")
	if !errors.Is(err, ErrRequestFailed) {
		t.Fatal(err)
	}
	_, err = c.Stat(context.Background(), "../bad")
	if err == nil {
		t.Fatal("invalid key accepted")
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err = c.Stat(ctx, "file")
	if !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}

func TestMethodIncludedInSignature(t *testing.T) {
	c, err := New(Config{BucketURL: "https://example.com/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret"})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	now := time.Date(2026, 1, 2, 0, 0, 0, 0, time.UTC)
	get, _ := c.presign(http.MethodGet, "file", nil, now, 60)
	head, _ := c.presign(http.MethodHead, "file", nil, now, 60)
	gu, _ := url.Parse(get)
	hu, _ := url.Parse(head)
	if gu.Query().Get("X-Amz-Signature") == hu.Query().Get("X-Amz-Signature") {
		t.Fatal("method not signed")
	}
}
