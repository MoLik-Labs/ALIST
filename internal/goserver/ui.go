package goserver

import (
	"crypto/sha256"
	"embed"
	"fmt"
	"net/http"
	"strings"
)

//go:embed ui/index.html ui/app.css ui/app.js
var uiFiles embed.FS

// serveUI serves only a fixed allowlist of embedded files; no filesystem paths
// are accepted from requests. It is mounted only when local storage is enabled.
func serveUI(w http.ResponseWriter, r *http.Request) bool {
	var file, contentType string
	switch r.URL.Path {
	case "/":
		file, contentType = "ui/index.html", "text/html; charset=utf-8"
	case "/app.js":
		file, contentType = "ui/app.js", "text/javascript; charset=utf-8"
	case "/app.css":
		file, contentType = "ui/app.css", "text/css; charset=utf-8"
	default:
		return false
	}
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "no-referrer")
	w.Header().Set("X-Frame-Options", "DENY")
	w.Header().Set("Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; frame-ancestors 'none'; form-action 'self'")
	w.Header().Set("Cache-Control", "no-cache")
	if r.Method != http.MethodGet && r.Method != http.MethodHead {
		w.Header().Set("Allow", "GET, HEAD")
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return true
	}
	body, err := uiFiles.ReadFile(file)
	if err != nil {
		http.Error(w, "UI unavailable", http.StatusInternalServerError)
		return true
	}
	sum := sha256.Sum256(body)
	etag := fmt.Sprintf("\"%x\"", sum)
	w.Header().Set("ETag", etag)
	w.Header().Set("Content-Type", contentType)
	for _, tag := range strings.Split(r.Header.Get("If-None-Match"), ",") {
		tag = strings.TrimSpace(tag)
		if tag == "*" || tag == etag || tag == "W/"+etag {
			w.WriteHeader(http.StatusNotModified)
			return true
		}
	}
	w.Header().Set("Content-Length", fmt.Sprint(len(body)))
	if r.Method != http.MethodHead {
		_, _ = w.Write(body)
	}
	return true
}
