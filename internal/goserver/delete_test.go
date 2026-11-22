package goserver

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestDeleteOptInAndWebDAV(t *testing.T) {
	root := t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "folder"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "folder", "file"), []byte("data"), 0600); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	readonly, err := LocalHandler(store, token)
	if err != nil {
		t.Fatal(err)
	}
	request := func(h http.Handler, method, target string, auth bool, headers map[string]string) int {
		t.Helper()
		req := httptest.NewRequest(method, target, nil)
		if auth {
			req.Header.Set("Authorization", "Bearer "+token)
		}
		for k, v := range headers {
			req.Header.Set(k, v)
		}
		w := httptest.NewRecorder()
		h.ServeHTTP(w, req)
		return w.Code
	}
	if status := request(readonly, "DELETE", "/api/local/file?path=/folder/file", true, nil); status != 405 {
		t.Fatal("default enabled", status)
	}
	handler, err := LocalHandlerWithPermissions(store, token, false, true)
	if err != nil {
		t.Fatal(err)
	}
	if status := request(handler, "DELETE", "/api/local/file?path=/folder/file", false, nil); status != 401 {
		t.Fatal("unauthenticated", status)
	}
	if status := request(handler, "DELETE", "/api/local/file?path=/folder", true, nil); status != 409 {
		t.Fatal("nonempty folder", status)
	}
	if status := request(handler, "DELETE", "/api/local/file?path=/folder/file", true, map[string]string{"If-Match": "*"}); status != 412 {
		t.Fatal("ignored precondition", status)
	}
	if status := request(handler, "DELETE", "/api/local/file?path=/", true, nil); status != 400 {
		t.Fatal("deleted root", status)
	}
	if status := request(handler, "DELETE", "/api/local/file?path=/folder/file", true, nil); status != 204 {
		t.Fatal("delete file", status)
	}
	dav, err := WebDAVHandlerWithPermissions(store, token, handler, false, true)
	if err != nil {
		t.Fatal(err)
	}
	if status := request(dav, "DELETE", "/dav/folder/", true, map[string]string{"Depth": "0"}); status != 400 {
		t.Fatal("invalid depth", status)
	}
	if status := request(dav, "DELETE", "/dav/folder/", true, nil); status != 204 {
		t.Fatal("delete empty collection", status)
	}
	if status := request(dav, "DELETE", "/dav/", true, nil); status != 400 {
		t.Fatal("delete root", status)
	}
}
