package goserver

import (
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestDAVResponseBounded(t *testing.T) {
	items := make([]davItem, 1001)
	for i := range items {
		items[i] = davItem{path: "/file", entry: localfs.Entry{Name: "file"}}
	}
	props := make([]xml.Name, 32)
	for i := range props {
		props[i] = xml.Name{Space: "urn:" + strings.Repeat("x", 250), Local: "unknown"}
	}
	_, err := davResponseXML(items, props, false)
	if !errors.Is(err, errDAVResponseTooLarge) {
		t.Fatalf("response not bounded: %v", err)
	}
}

func TestDAVRawDirectoryLimit(t *testing.T) {
	root := t.TempDir()
	for i := 0; i < 1001; i++ {
		if err := os.WriteFile(filepath.Join(root, fmt.Sprintf("f%04d", i)), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	store, err := localfs.Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("a", 32)
	h, err := WebDAVHandler(store, token, http.NotFoundHandler())
	if err != nil {
		t.Fatal(err)
	}
	req := httptest.NewRequest("PROPFIND", "/dav/", nil)
	req.Header.Set("Depth", "1")
	req.SetBasicAuth("kova", token)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, req)
	if w.Code != http.StatusInsufficientStorage {
		t.Fatalf("status = %d", w.Code)
	}
}

type waitingDAVBody struct {
	entered chan<- struct{}
	release <-chan struct{}
}

func (b waitingDAVBody) Read(_ []byte) (int, error) {
	b.entered <- struct{}{}
	<-b.release
	return 0, io.EOF
}
func (b waitingDAVBody) Close() error { return nil }

func TestDAVPropfindConcurrencyLimit(t *testing.T) {
	store, err := localfs.Open(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	token := strings.Repeat("a", 32)
	h, err := WebDAVHandler(store, token, http.NotFoundHandler())
	if err != nil {
		t.Fatal(err)
	}
	entered := make(chan struct{}, 8)
	release := make(chan struct{})
	done := make(chan int, 8)
	for i := 0; i < 8; i++ {
		go func() {
			req := httptest.NewRequest("PROPFIND", "/dav/", nil)
			req.Body = waitingDAVBody{entered, release}
			req.Header.Set("Depth", "0")
			req.SetBasicAuth("kova", token)
			w := httptest.NewRecorder()
			h.ServeHTTP(w, req)
			done <- w.Code
		}()
	}
	for i := 0; i < 8; i++ {
		<-entered
	}
	req := httptest.NewRequest("PROPFIND", "/dav/", nil)
	req.Header.Set("Depth", "0")
	req.SetBasicAuth("kova", token)
	w := httptest.NewRecorder()
	h.ServeHTTP(w, req)
	close(release)
	for i := 0; i < 8; i++ {
		if status := <-done; status != 207 {
			t.Errorf("pending status %d", status)
		}
	}
	if w.Code != 429 {
		t.Fatalf("excess status %d", w.Code)
	}
}
