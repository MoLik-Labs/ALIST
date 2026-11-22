//go:build linux && (amd64 || arm64)

package goserver

import (
	"crypto/tls"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestWebDAVMove(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	for n, value := range map[string]string{"a": "hello", "occupied": "keep"} {
		if err := os.WriteFile(filepath.Join(root, n), []byte(value), 0600); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Symlink(outside, filepath.Join(root, "link")); err != nil {
		t.Fatal(err)
	}
	s, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	token := strings.Repeat("x", 32)
	disabled, err := WebDAVHandlerWithPermissions(s, token, http.NotFoundHandler(), false, false)
	if err != nil {
		t.Fatal(err)
	}
	enabled, err := WebDAVHandlerWithMove(s, token, http.NotFoundHandler(), false, false, true)
	if err != nil {
		t.Fatal(err)
	}
	req := func(h http.Handler, headers map[string]string, auth bool) int {
		r := httptest.NewRequest("MOVE", "http://example.com/dav/a", nil)
		if auth {
			r.SetBasicAuth("kova", token)
		}
		for k, v := range headers {
			r.Header.Set(k, v)
		}
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		return w.Code
	}
	for _, tc := range []struct {
		name    string
		h       http.Handler
		headers map[string]string
		auth    bool
		want    int
	}{
		{"disabled", disabled, map[string]string{"Destination": "/dav/b"}, true, 405},
		{"auth", enabled, map[string]string{"Destination": "/dav/b"}, false, 401},
		{"missing destination", enabled, nil, true, 400},
		{"foreign origin", enabled, map[string]string{"Destination": "http://evil.example/dav/b"}, true, 400},
		{"foreign scheme", enabled, map[string]string{"Destination": "https://example.com/dav/b"}, true, 400},
		{"outside", enabled, map[string]string{"Destination": "/api/local/b"}, true, 400},
		{"traversal", enabled, map[string]string{"Destination": "/dav/%2e%2e/b"}, true, 400},
		{"query", enabled, map[string]string{"Destination": "/dav/b?q=1"}, true, 400},
		{"symlink", enabled, map[string]string{"Destination": "/dav/link/b"}, true, 409},
		{"depth", enabled, map[string]string{"Destination": "/dav/b", "Depth": "0"}, true, 400},
		{"invalid overwrite", enabled, map[string]string{"Destination": "/dav/b", "Overwrite": "bad"}, true, 400},
		{"lock condition", enabled, map[string]string{"Destination": "/dav/b", "If": "(<lock>)"}, true, 412},
		{"exists F", enabled, map[string]string{"Destination": "/dav/occupied", "Overwrite": "F"}, true, 412},
		{"exists T", enabled, map[string]string{"Destination": "/dav/occupied"}, true, 403},
		{"success", enabled, map[string]string{"Destination": "http://example.com/dav/b", "Overwrite": "F"}, true, 201},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if got := req(tc.h, tc.headers, tc.auth); got != tc.want {
				t.Errorf("status %d want %d", got, tc.want)
			}
		})
	}
	for n, want := range map[string]string{"b": "hello", "occupied": "keep"} {
		b, err := os.ReadFile(filepath.Join(root, n))
		if err != nil || string(b) != want {
			t.Fatalf("%s: %q %v", n, b, err)
		}
	}
}

func TestDAVMoveDestination(t *testing.T) {
	for _, tc := range []struct {
		destination string
		tls         bool
		ok          bool
		path        string
	}{
		{"/dav/name%20with%20spaces", false, true, "/name with spaces"},
		{"http://example.com/dav/x", false, true, "/x"},
		{"https://example.com/dav/x", true, true, "/x"},
		{"https://example.com/dav/x", false, false, ""},
		{"//evil.example/dav/x", false, false, ""},
		{"/dav2/x", false, false, ""},
		{"/dav/x#fragment", false, false, ""},
		{"http://user@example.com/dav/x", false, false, ""},
		{"/dav/x?", false, false, ""},
	} {
		t.Run(tc.destination, func(t *testing.T) {
			r := httptest.NewRequest("MOVE", "http://example.com/dav/source", nil)
			if tc.tls {
				r.TLS = &tls.ConnectionState{}
			}
			r.Header.Set("Destination", tc.destination)
			got, err := davMoveDestination(r)
			if (err == nil) != tc.ok || (tc.ok && got != tc.path) {
				t.Fatalf("got %q, %v", got, err)
			}
		})
	}
	r := httptest.NewRequest("MOVE", "http://example.com/dav/x", nil)
	r.Header.Add("Destination", "/dav/a")
	r.Header.Add("Destination", "/dav/b")
	if _, err := davMoveDestination(r); err == nil {
		t.Fatal("duplicate destination accepted")
	}
}
