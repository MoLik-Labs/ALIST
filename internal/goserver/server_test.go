package goserver

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
)

func TestHandlerFailsClosed(t *testing.T) {
	srv := httptest.NewServer(Handler())
	defer srv.Close()
	for _, tc := range []struct {
		method, path string
		status       int
	}{
		{"GET", "/ping", 200}, {"HEAD", "/ping", 200}, {"POST", "/ping", 405},
		{"GET", "/", 503}, {"GET", "/ready", 503}, {"POST", "/api/auth/login", 503},
		{"GET", "/d/private.txt", 503}, {"PROPFIND", "/dav", 503},
	} {
		req, _ := http.NewRequest(tc.method, srv.URL+tc.path, nil)
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatal(err)
		}
		body, _ := io.ReadAll(resp.Body)
		resp.Body.Close()
		if resp.StatusCode != tc.status {
			t.Errorf("%s %s: %d", tc.method, tc.path, resp.StatusCode)
		}
		if tc.status == 503 && (!strings.Contains(string(body), "not implemented") || resp.Header.Get("Cache-Control") != "no-store") {
			t.Errorf("unsafe error response: %q", body)
		}
		if tc.method == "HEAD" && len(body) != 0 {
			t.Errorf("HEAD returned body: %q", body)
		}
	}
}

func TestConcurrentRequests(t *testing.T) {
	srv := httptest.NewServer(Handler())
	defer srv.Close()
	var wg sync.WaitGroup
	for i := 0; i < 16; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 20; j++ {
				resp, err := http.Get(srv.URL + "/ping")
				if err != nil {
					t.Error(err)
					return
				}
				_, _ = io.Copy(io.Discard, resp.Body)
				resp.Body.Close()
				if resp.StatusCode != 200 {
					t.Errorf("status: %d", resp.StatusCode)
				}
			}
		}()
	}
	wg.Wait()
}
