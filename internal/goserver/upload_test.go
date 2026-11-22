package goserver

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestWriteOptIn(t *testing.T) {
	root := t.TempDir()
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	handler, err := LocalHandlerWithWrite(store, token, true)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()
	put := func(path, body, auth string) (int, string) {
		t.Helper()
		req, _ := http.NewRequest("PUT", server.URL+path, strings.NewReader(body))
		if auth != "" {
			req.Header.Set("Authorization", "Bearer "+auth)
		}
		res, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatal(err)
		}
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		return res.StatusCode, string(b)
	}
	if status, _ := put("/api/local/file?path=/new", "abc", ""); status != 401 {
		t.Fatal(status)
	}
	if status, body := put("/api/local/file?path=/new", "abc", token); status != 201 || !strings.Contains(body, `"size":3`) {
		t.Fatal(status, body)
	}
	if status, _ := put("/api/local/file?path=/new", "changed", token); status != 409 {
		t.Fatal(status)
	}
	if status, _ := put("/api/local/file?path=/%2e%2e/escape", "bad", token); status != 400 {
		t.Fatal(status)
	}
	if b, err := os.ReadFile(filepath.Join(root, "new")); err != nil || string(b) != "abc" {
		t.Fatal(string(b), err)
	}
	mkdir := func(target string) int {
		t.Helper()
		req, _ := http.NewRequest("POST", server.URL+target, nil)
		req.Header.Set("Authorization", "Bearer "+token)
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatal(err)
		}
		resp.Body.Close()
		return resp.StatusCode
	}
	if status := mkdir("/api/local/mkdir?path=/folder"); status != 201 {
		t.Fatal("mkdir", status)
	}
	if status := mkdir("/api/local/mkdir?path=/folder"); status != 409 {
		t.Fatal("duplicate mkdir", status)
	}
	if status := mkdir("/api/local/mkdir?path=/%2e%2e/bad"); status != 400 {
		t.Fatal("mkdir traversal", status)
	}
	req := httptest.NewRequest("PUT", "/api/local/file?path=/big", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	req.ContentLength = (64 << 20) + 1
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)
	if w.Code != 413 {
		t.Fatal("oversized upload", w.Code)
	}
}
