package goserver

import (
	"encoding/xml"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func TestReadOnlyWebDAV(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "folder"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "folder", "a & b.txt"), []byte("hello world"), 0600); err != nil {
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
	token := strings.Repeat("x", 32)
	local, _ := LocalHandler(store, token)
	handler, err := WebDAVHandler(store, token, local)
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()
	request := func(method, target, depth, body string, authenticated bool, headers map[string]string) (int, http.Header, string) {
		t.Helper()
		req, _ := http.NewRequest(method, server.URL+target, strings.NewReader(body))
		if authenticated {
			req.SetBasicAuth("kova", token)
		}
		if depth != "" {
			req.Header.Set("Depth", depth)
		}
		for k, v := range headers {
			req.Header.Set(k, v)
		}
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Fatal(err)
		}
		b, _ := io.ReadAll(resp.Body)
		resp.Body.Close()
		return resp.StatusCode, resp.Header, string(b)
	}
	if status, headers, _ := request("PROPFIND", "/dav/", "1", "", false, nil); status != 401 || !strings.Contains(headers.Get("WWW-Authenticate"), "Basic") {
		t.Fatal(status, headers)
	}
	status, headers, body := request("PROPFIND", "/dav/folder/", "1", "", true, nil)
	if status != 207 || !strings.Contains(headers.Get("Content-Type"), "application/xml") || !strings.Contains(body, "/dav/folder/a%20&amp;%20b.txt") {
		t.Fatal(status, body)
	}
	var parsed struct {
		XMLName   xml.Name   `xml:"DAV: multistatus"`
		Responses []struct{} `xml:"DAV: response"`
	}
	if err := xml.Unmarshal([]byte(body), &parsed); err != nil || len(parsed.Responses) != 2 {
		t.Fatal("invalid multistatus", err, len(parsed.Responses), body)
	}
	query := `<D:propfind xmlns:D="DAV:" xmlns:X="urn:test"><D:prop><D:displayname/><X:missing/></D:prop></D:propfind>`
	status, _, body = request("PROPFIND", "/dav/folder/", "0", query, true, nil)
	if status != 207 || !strings.Contains(body, "200 OK") || !strings.Contains(body, "404 Not Found") {
		t.Fatal(status, body)
	}
	status, _, body = request("GET", "/dav/folder/a%20%26%20b.txt", "", "", true, map[string]string{"Range": "bytes=1-4"})
	if status != 206 || body != "ello" {
		t.Fatal(status, body)
	}
	for _, target := range []string{"/dav/escape/secret", "/dav/%2e%2e/secret", "/dav/.kova-upload-secret"} {
		status, _, body := request("GET", target, "", "", true, nil)
		if status == 200 || strings.Contains(body, "secret") {
			t.Fatal("escape", target, status, body)
		}
	}
	if status, _, _ := request("PROPFIND", "/dav/", "infinity", "", true, nil); status != 403 {
		t.Fatal("infinite depth", status)
	}
	if status, _, _ := request("PROPFIND", "/dav/", "1", `<propfind>`, true, nil); status != 400 {
		t.Fatal("invalid XML", status)
	}
	if status, _, _ := request("PROPFIND", "/dav/", "0", `<D:propfind xmlns:D="DAV:"><D:allprop/></D:propfind><extra/>`, true, nil); status != 400 {
		t.Fatal("multiple XML roots", status)
	}
	if status, _, _ := request("PUT", "/dav/new", "", "data", true, nil); status != 405 {
		t.Fatal("write enabled", status)
	}
	if status, _, _ := request("OPTIONS", "/dav/", "", "", true, nil); status != 204 {
		t.Fatal("options", status)
	}
	if status, _, _ := request("PROPFIND", "/dav/", "0", `<D:propfind xmlns:D="DAV:"><D:propname/></D:propfind>`, false, map[string]string{"Authorization": "Bearer " + token}); status != 207 {
		t.Fatal("bearer or propname", status)
	}
	longNS := strings.Repeat("x", 257)
	xmlBody := `<D:propfind xmlns:D="DAV:" xmlns:X="` + longNS + `"><D:prop><X:prop/></D:prop></D:propfind>`
	if status, _, _ := request("PROPFIND", "/dav/", "1", xmlBody, true, nil); status != 400 {
		t.Fatal("oversized namespace accepted", status)
	}
	if status, _, _ := request("GET", "/", "", "", false, nil); status != 200 {
		t.Fatal("fallback", status)
	}
}
