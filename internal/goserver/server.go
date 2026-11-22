// Package goserver is the incomplete Go server entrypoint for the migration.
// Unimplemented endpoints fail closed rather than presenting a working service.
package goserver

import (
	"encoding/json"
	"net/http"
	"time"
)

const Version = "v4.2.0"

// Handler exposes only liveness and an explicit not-ready status. It never
// opens or modifies an existing data directory and does not serve the app UI.
func Handler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Cache-Control", "no-store")
		if r.URL.Path == "/ping" && (r.Method == http.MethodGet || r.Method == http.MethodHead) {
			w.Header().Set("Content-Type", "text/plain; charset=utf-8")
			_, _ = w.Write([]byte("pong"))
			return
		}
		if r.URL.Path == "/ping" {
			w.Header().Set("Allow", "GET, HEAD")
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		w.Header().Set("Retry-After", "60")
		w.WriteHeader(http.StatusServiceUnavailable)
		_ = json.NewEncoder(w).Encode(struct {
			Code    int    `json:"code"`
			Message string `json:"message"`
		}{http.StatusServiceUnavailable, "Go migration in progress; this endpoint is not implemented"})
	})
}

// New constructs a bounded-header server. No write timeout is set because a
// future full server must support streaming; request handlers must enforce their
// own deadlines and limits. Call Shutdown for a graceful stop.
func New(addr string) *http.Server {
	return &http.Server{
		Addr:              addr,
		Handler:           Handler(),
		ReadHeaderTimeout: 10 * time.Second,
		IdleTimeout:       60 * time.Second,
		MaxHeaderBytes:    32 << 10,
	}
}
