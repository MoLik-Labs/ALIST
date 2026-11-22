//go:build linux && (amd64 || arm64)

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

func TestLocalMovePermissionsAndValidation(t *testing.T) {
	root := t.TempDir()
	for _, n := range []string{"a", "existing"} {
		if err := os.WriteFile(filepath.Join(root, n), []byte(n), 0600); err != nil {
			t.Fatal(err)
		}
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	readonly, _ := LocalHandler(store, token)
	enabled, _ := LocalHandlerWithMove(store, token, false, false, true)
	call := func(h interface {
		ServeHTTP(http.ResponseWriter, *http.Request)
	}, target string, auth bool, headers map[string]string) int {
		req := httptest.NewRequest("POST", target, nil)
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
	for _, tc := range []struct {
		h       http.Handler
		target  string
		auth    bool
		headers map[string]string
		want    int
	}{
		{readonly, "/api/local/move?path=/a&destination=/b", true, nil, 405},
		{enabled, "/api/local/move?path=/a&destination=/b", false, nil, 401},
		{enabled, "/api/local/move?path=/a&destination=/b&destination=/c", true, nil, 400},
		{enabled, "/api/local/move?path=/a&destination=/b", true, map[string]string{"If-Match": "*"}, 412},
		{enabled, "/api/local/move?path=/a&destination=/existing", true, nil, 409},
		{enabled, "/api/local/move?path=/a&destination=/%2e%2e/out", true, nil, 400},
		{enabled, "/api/local/move?path=/a&destination=/b", true, nil, 204},
	} {
		if got := call(tc.h, tc.target, tc.auth, tc.headers); got != tc.want {
			t.Errorf("%s: %d want %d", tc.target, got, tc.want)
		}
	}
	if b, err := os.ReadFile(filepath.Join(root, "existing")); err != nil || string(b) != "existing" {
		t.Fatal("overwrite", err)
	}
	if b, err := os.ReadFile(filepath.Join(root, "b")); err != nil || string(b) != "a" {
		t.Fatal("move", err)
	}
}
