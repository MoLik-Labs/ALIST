package goserver

import (
	"encoding/json"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestLocalCapabilities(t *testing.T) {
	store, err := localfs.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	for _, tc := range []struct{ create, delete bool }{{false, false}, {true, false}, {false, true}, {true, true}} {
		h, err := LocalHandlerWithPermissions(store, token, tc.create, tc.delete)
		if err != nil {
			t.Fatal(err)
		}
		r := httptest.NewRequest("GET", "/api/local/list?path=/", nil)
		r.Header.Set("Authorization", "Bearer "+token)
		w := httptest.NewRecorder()
		h.ServeHTTP(w, r)
		var data struct {
			Capabilities localCapabilities `json:"capabilities"`
		}
		if w.Code != 200 {
			t.Fatalf("status %d", w.Code)
		}
		if err := json.Unmarshal(w.Body.Bytes(), &data); err != nil {
			t.Fatal(err)
		}
		if data.Capabilities.Create != tc.create || data.Capabilities.Delete != tc.delete || data.Capabilities.Share {
			t.Fatalf("unexpected capabilities: %+v", data.Capabilities)
		}
		unauth := httptest.NewRecorder()
		h.ServeHTTP(unauth, httptest.NewRequest("GET", "/api/local/list?path=/", nil))
		if unauth.Code != 401 {
			t.Fatal("unauthorized status", unauth.Code)
		}
	}
}

func TestShareCapabilityOptIn(t *testing.T) {
	store, err := localfs.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("x", 32)
	h, err := LocalHandlerWithFeatures(store, token, false, false, false, true)
	if err != nil {
		t.Fatal(err)
	}
	req := httptest.NewRequest("GET", "/api/local/list?path=/", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, req)
	var data struct {
		Capabilities localCapabilities `json:"capabilities"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &data); err != nil || w.Code != 200 || !data.Capabilities.Share {
		t.Fatal(w.Code, data, err)
	}
}
