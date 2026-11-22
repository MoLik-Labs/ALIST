package goserver

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestSharesLifecycle(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "file"), []byte("hello"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Mkdir(filepath.Join(root, "dir"), 0700); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	h, err := ShareHandler(store, token, http.NotFoundHandler())
	if err != nil {
		t.Fatal(err)
	}
	call := func(method, target, body string, admin bool, headers map[string]string) *httptest.ResponseRecorder {
		r := httptest.NewRequest(method, target, strings.NewReader(body))
		if admin {
			r.Header.Set("Authorization", "Bearer "+token)
		}
		for k, v := range headers {
			r.Header.Set(k, v)
		}
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		return w
	}
	if w := call("POST", "/api/local/shares", `{"path":"/file","ttl_seconds":60}`, false, nil); w.Code != 401 {
		t.Fatal(w.Code)
	}
	for _, body := range []string{`{"path":"/../file","ttl_seconds":60}`, `{"path":"/dir","ttl_seconds":60}`, `{"path":"/file","ttl_seconds":1}`, `{"path":"/file","ttl_seconds":999999999}`, `{"path":"/file","ttl_seconds":60,"extra":true}`, `{"path":"/file","ttl_seconds":60}{}`} {
		if w := call("POST", "/api/local/shares", body, true, nil); w.Code == 201 {
			t.Fatalf("accepted %s", body)
		}
	}
	w := call("POST", "/api/local/shares", `{"path":"/file","ttl_seconds":60}`, true, nil)
	if w.Code != 201 {
		t.Fatal(w.Code, w.Body.String())
	}
	var result struct {
		URL     string `json:"url"`
		Expires int64  `json:"expires"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &result); err != nil {
		t.Fatal(err)
	}
	if !strings.HasPrefix(result.URL, "/s/") || result.Expires <= time.Now().Unix() {
		t.Fatal(result)
	}
	w = call("GET", result.URL, "", false, map[string]string{"Range": "bytes=1-3"})
	if w.Code != 206 || w.Body.String() != "ell" || w.Header().Get("Cache-Control") != "no-store" {
		t.Fatal(w.Code, w.Body.String(), w.Header())
	}
	if w := call("POST", result.URL, "", false, nil); w.Code != 405 {
		t.Fatal(w.Code)
	}
	if w := call("DELETE", "/api/local/shares/"+strings.TrimPrefix(result.URL, "/s/"), "", false, nil); w.Code != 401 {
		t.Fatal(w.Code)
	}
	if w := call("DELETE", "/api/local/shares/"+strings.TrimPrefix(result.URL, "/s/"), "", true, nil); w.Code != 204 {
		t.Fatal(w.Code)
	}
	if w := call("GET", result.URL, "", false, nil); w.Code != 404 {
		t.Fatal(w.Code)
	}
}

func TestShareReplacementAndExpiration(t *testing.T) {
	root := t.TempDir()
	file := filepath.Join(root, "file")
	if err := os.WriteFile(file, []byte("old"), 0600); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("a", 32)
	h, err := ShareHandler(store, token, http.NotFoundHandler())
	if err != nil {
		t.Fatal(err)
	}
	r := httptest.NewRequest("POST", "/api/local/shares", bytes.NewBufferString(`{"path":"/file","ttl_seconds":60}`))
	r.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	var x struct {
		URL string `json:"url"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &x); err != nil || w.Code != 201 {
		t.Fatal(w.Code, err)
	}
	if err := os.Rename(file, file+".old"); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(file, []byte("new"), 0600); err != nil {
		t.Fatal(err)
	}
	w = httptest.NewRecorder()
	h.ServeHTTP(w, httptest.NewRequest("GET", x.URL, nil))
	if w.Code != 404 {
		t.Fatal("replacement leaked", w.Code)
	}
}

func TestShareExpiryAndLimit(t *testing.T) {
	current := time.Unix(1800000000, 0)
	registry := &shareRegistry{entries: make(map[string]sharedFile), now: func() time.Time { return current }}
	for i := 0; i < maxShares; i++ {
		if _, err := registry.add(sharedFile{expires: current.Add(time.Minute)}); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := registry.add(sharedFile{expires: current.Add(time.Minute)}); err == nil {
		t.Fatal("limit not enforced")
	}
	current = current.Add(time.Minute)
	if _, err := registry.add(sharedFile{expires: current.Add(time.Minute)}); err != nil {
		t.Fatal("expired entries not pruned", err)
	}
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "file"), []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	h, err := shareHandlerWithClock(store, token, http.NotFoundHandler(), func() time.Time { return current })
	if err != nil {
		t.Fatal(err)
	}
	r := httptest.NewRequest("POST", "/api/local/shares", strings.NewReader(`{"path":"/file","ttl_seconds":60}`))
	r.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, r)
	var result struct {
		URL string `json:"url"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &result); err != nil || w.Code != 201 {
		t.Fatal(w.Code, err)
	}
	current = current.Add(time.Minute)
	w = httptest.NewRecorder()
	h.ServeHTTP(w, httptest.NewRequest("GET", result.URL, nil))
	if w.Code != 404 {
		t.Fatal("expired link", w.Code)
	}
}

func TestShareRegistryConcurrent(t *testing.T) {
	registry := &shareRegistry{entries: make(map[string]sharedFile), now: time.Now}
	var wg sync.WaitGroup
	for i := 0; i < 16; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 50; j++ {
				id, err := registry.add(sharedFile{expires: time.Now().Add(time.Minute)})
				if err != nil {
					t.Error(err)
					return
				}
				if _, ok := registry.get(id); !ok {
					t.Error("missing share")
				}
				_ = registry.list()
				if !registry.revoke(id) {
					t.Error("revoke failed")
				}
			}
		}()
	}
	wg.Wait()
}

func TestShareListAuthorizationAndExpiry(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "f"), []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	now := time.Unix(1800000000, 0)
	h, err := shareHandlerWithClock(store, token, http.NotFoundHandler(), func() time.Time { return now })
	if err != nil {
		t.Fatal(err)
	}
	call := func(method, body string, auth bool) *httptest.ResponseRecorder {
		r := httptest.NewRequest(method, "/api/local/shares", strings.NewReader(body))
		if auth {
			r.Header.Set("Authorization", "Bearer "+token)
		}
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		return w
	}
	if w := call("GET", "", false); w.Code != 401 {
		t.Fatal("unauthorized list", w.Code)
	}
	if w := call("POST", `{"path":"/f","ttl_seconds":60}`, true); w.Code != 201 {
		t.Fatal(w.Code)
	}
	w := call("GET", "", true)
	var data struct {
		Shares []struct {
			Path    string `json:"path"`
			URL     string `json:"url"`
			Expires int64  `json:"expires"`
		} `json:"shares"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &data); err != nil || w.Code != 200 || len(data.Shares) != 1 || data.Shares[0].Path != "/f" || !strings.HasPrefix(data.Shares[0].URL, "/s/") || w.Header().Get("Cache-Control") != "no-store" {
		t.Fatal(w.Code, w.Body.String(), err)
	}
	now = now.Add(time.Minute)
	w = call("GET", "", true)
	if err := json.Unmarshal(w.Body.Bytes(), &data); err != nil || len(data.Shares) != 0 {
		t.Fatal("expired list", w.Body.String(), err)
	}
}
