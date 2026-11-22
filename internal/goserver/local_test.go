package goserver

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestAuthenticatedLocalAPI(t *testing.T) {
	root := t.TempDir()
	outside := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "file.txt"), []byte("hello world"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(outside, "secret"), []byte("secret"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "escape")); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	if _, err := LocalHandler(store, "short"); err == nil {
		t.Fatal("short token accepted")
	}
	token := strings.Repeat("x", 32)
	h, err := LocalHandler(store, token)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(h)
	defer server.Close()
	request := func(method, url, key, rangeHeader string) (int, http.Header, string) {
		t.Helper()
		req, _ := http.NewRequest(method, server.URL+url, nil)
		if key != "" {
			req.Header.Set("Authorization", "Bearer "+key)
		}
		if rangeHeader != "" {
			req.Header.Set("Range", rangeHeader)
		}
		res, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatal(err)
		}
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		return res.StatusCode, res.Header, string(b)
	}
	if status, _, _ := request("GET", "/api/local/list?path=/", "", ""); status != 401 {
		t.Fatal("unauthenticated", status)
	}
	if status, _, _ := request("GET", "/api/local/list?path=/", strings.Repeat("z", 32), ""); status != 401 {
		t.Fatal("wrong token", status)
	}
	status, _, body := request("GET", "/api/local/list?path=/", token, "")
	var listing struct {
		Entries []localfs.Entry `json:"entries"`
	}
	if status != 200 || json.Unmarshal([]byte(body), &listing) != nil || len(listing.Entries) != 1 || listing.Entries[0].Name != "file.txt" {
		t.Fatal(status, body)
	}
	status, headers, body := request("GET", "/api/local/file?path=/file.txt", token, "bytes=1-4")
	if status != 206 || body != "ello" || !strings.HasPrefix(headers.Get("Content-Disposition"), "attachment") || headers.Get("Content-Type") != "application/octet-stream" {
		t.Fatal(status, headers, body)
	}
	for _, url := range []string{"/api/local/file?path=/escape/secret", "/api/local/file?path=/%2e%2e/secret", "/api/local/file?path=/file.txt&path=/escape/secret", "/api/local/file?path=%zz"} {
		status, _, body = request("GET", url, token, "")
		if status == 200 || strings.Contains(body, "secret") {
			t.Fatal("escape", url, status, body)
		}
	}
	if status, _, _ = request("PUT", "/api/local/file?path=/file.txt", token, ""); status != 405 {
		t.Fatal("write allowed", status)
	}
	if status, _, _ = request("GET", "/api/fs/list", token, ""); status != 503 {
		t.Fatal("unsupported API did not fail closed", status)
	}
}
