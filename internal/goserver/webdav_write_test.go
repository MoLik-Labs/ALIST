package goserver

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestWebDAVWrite(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	if err := os.Symlink(outside, filepath.Join(root, "escape")); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	next, _ := LocalHandler(store, token)
	handler, err := WebDAVHandlerWithWrite(store, token, next, true)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()
	call := func(method, target, body string, headers map[string]string) int {
		t.Helper()
		req, err := http.NewRequest(method, server.URL+target, strings.NewReader(body))
		if err != nil {
			t.Fatal(err)
		}
		req.SetBasicAuth("kova", token)
		for k, v := range headers {
			req.Header.Set(k, v)
		}
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Error(err)
			return 0
		}
		_, _ = io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
		return resp.StatusCode
	}
	for _, tc := range []struct {
		method, target, body string
		headers              map[string]string
		status               int
	}{
		{"MKCOL", "/dav/folder/", "", nil, 201},
		{"MKCOL", "/dav/folder/", "", nil, 405},
		{"MKCOL", "/dav/folder/", "", map[string]string{"If-None-Match": "*"}, 412},
		{"MKCOL", "/dav/missing/nested", "", nil, 409},
		{"MKCOL", "/dav/body", "x", nil, 415},
		{"MKCOL", "/dav/escape/outside", "", nil, 409},
		{"PUT", "/dav/folder/file.txt", "hello", nil, 201},
		{"PUT", "/dav/folder/file.txt", "changed", nil, 409},
		{"PUT", "/dav/folder/file.txt", "changed", map[string]string{"If-None-Match": "*"}, 412},
		{"PUT", "/dav/folder/unknown", "data", map[string]string{"If-Match": "*"}, 412},
		{"PUT", "/dav/folder/unknown", "data", map[string]string{"If": "(<opaquelocktoken:xxx>)"}, 412},
		{"PUT", "/dav/folder/unknown", "data", map[string]string{"Content-Range": "bytes 0-3/4"}, 501},
		{"PUT", "/dav/missing/file", "data", nil, 409},
		{"PUT", "/dav/folder/", "data", nil, 405},
		{"PUT", "/dav/%2e%2e/unsafe", "data", nil, 400},
		{"DELETE", "/dav/folder/file.txt", "", nil, 405},
	} {
		if status := call(tc.method, tc.target, tc.body, tc.headers); status != tc.status {
			t.Errorf("%s %s: got %d want %d", tc.method, tc.target, status, tc.status)
		}
	}
	if b, err := os.ReadFile(filepath.Join(root, "folder", "file.txt")); err != nil || string(b) != "hello" {
		t.Fatal("overwritten", string(b), err)
	}
	if _, err := os.Stat(filepath.Join(outside, "outside")); !os.IsNotExist(err) {
		t.Fatal("escaped root", err)
	}
	var success atomic.Int32
	var wg sync.WaitGroup
	for i := 0; i < 12; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			status := call("PUT", "/dav/folder/race", "same", nil)
			if status == 201 {
				success.Add(1)
			} else if status != 409 && status != 429 {
				t.Errorf("concurrent upload status %d", status)
			}
		}()
	}
	wg.Wait()
	if success.Load() != 1 {
		t.Fatal("concurrent success count", success.Load())
	}
}
