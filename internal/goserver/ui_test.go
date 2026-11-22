package goserver

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestEmbeddedUI(t *testing.T) {
	store, err := localfs.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	handler, err := LocalHandler(store, strings.Repeat("x", 32))
	if err != nil {
		t.Fatal(err)
	}
	for _, tc := range []struct{ path, mime string }{{"/", "text/html"}, {"/app.js", "text/javascript"}, {"/app.css", "text/css"}} {
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, httptest.NewRequest("GET", tc.path, nil))
		if w.Code != 200 || !strings.HasPrefix(w.Header().Get("Content-Type"), tc.mime) || w.Header().Get("ETag") == "" || !strings.Contains(w.Header().Get("Content-Security-Policy"), "frame-ancestors 'none'") {
			t.Fatalf("%s: %d %v", tc.path, w.Code, w.Header())
		}
		etag := w.Header().Get("ETag")
		conditional := httptest.NewRequest("GET", tc.path, nil)
		conditional.Header.Set("If-None-Match", "W/"+etag)
		w = httptest.NewRecorder()
		handler.ServeHTTP(w, conditional)
		if w.Code != http.StatusNotModified || w.Body.Len() != 0 {
			t.Fatal("conditional request", w.Code)
		}
		w = httptest.NewRecorder()
		handler.ServeHTTP(w, httptest.NewRequest("HEAD", tc.path, nil))
		if w.Code != 200 || w.Body.Len() != 0 || w.Header().Get("Content-Length") == "" {
			t.Fatal("HEAD", w.Code)
		}
	}
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("POST", "/", nil))
	if w.Code != http.StatusMethodNotAllowed {
		t.Fatal(w.Code)
	}
	w = httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("GET", "/api/not-implemented", nil))
	if w.Code != http.StatusServiceUnavailable {
		t.Fatal(w.Code)
	}
}
