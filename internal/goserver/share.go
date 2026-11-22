package goserver

import (
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"mime"
	"net/http"
	"os"
	"path"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

const maxShares = 1000

type sharedFile struct {
	path    string
	info    os.FileInfo
	expires time.Time
}
type shareRegistry struct {
	mu      sync.Mutex
	entries map[string]sharedFile
	now     func() time.Time
}

func (s *shareRegistry) get(id string) (sharedFile, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	item, ok := s.entries[id]
	if ok && !s.now().Before(item.expires) {
		delete(s.entries, id)
		return sharedFile{}, false
	}
	return item, ok
}
func (s *shareRegistry) add(item sharedFile) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for id, old := range s.entries {
		if !s.now().Before(old.expires) {
			delete(s.entries, id)
		}
	}
	if len(s.entries) >= maxShares {
		return "", errors.New("share limit reached")
	}
	for i := 0; i < 3; i++ {
		var b [32]byte
		if _, err := rand.Read(b[:]); err != nil {
			return "", err
		}
		id := base64.RawURLEncoding.EncodeToString(b[:])
		if _, exists := s.entries[id]; !exists {
			s.entries[id] = item
			return id, nil
		}
	}
	return "", errors.New("share ID collision")
}

type shareListItem struct {
	Path    string `json:"path"`
	URL     string `json:"url"`
	Expires int64  `json:"expires"`
}

func (s *shareRegistry) list() []shareListItem {
	s.mu.Lock()
	items := make([]shareListItem, 0, len(s.entries))
	now := s.now()
	for id, entry := range s.entries {
		if !now.Before(entry.expires) {
			delete(s.entries, id)
			continue
		}
		items = append(items, shareListItem{Path: entry.path, URL: "/s/" + id, Expires: entry.expires.Unix()})
	}
	s.mu.Unlock()
	sort.Slice(items, func(i, j int) bool {
		if items[i].Expires != items[j].Expires {
			return items[i].Expires < items[j].Expires
		}
		return items[i].URL < items[j].URL
	})
	return items
}

func (s *shareRegistry) revoke(id string) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.entries[id]; !ok {
		return false
	}
	delete(s.entries, id)
	return true
}

// ShareHandler adds optional, in-memory, bearer-capability file shares. A
// restart revokes them all. Existing requests may finish after revocation.
func ShareHandler(store *localfs.Store, token string, next http.Handler) (http.Handler, error) {
	return shareHandlerWithClock(store, token, next, time.Now)
}
func shareHandlerWithClock(store *localfs.Store, token string, next http.Handler, now func() time.Time) (http.Handler, error) {
	if store == nil || next == nil || now == nil || len(token) < 32 || len(token) > 4096 || strings.ContainsAny(token, "\r\n\x00") {
		return nil, errors.New("invalid share configuration")
	}
	key := sha256.Sum256([]byte(token))
	registry := &shareRegistry{entries: make(map[string]sharedFile), now: now}
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		isAdmin := r.URL.Path == "/api/local/shares" || strings.HasPrefix(r.URL.Path, "/api/local/shares/")
		isPublic := strings.HasPrefix(r.URL.Path, "/s/")
		if !isAdmin && !isPublic {
			next.ServeHTTP(w, r)
			return
		}
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Referrer-Policy", "no-referrer")
		w.Header().Set("Content-Security-Policy", "default-src 'none'; sandbox")
		if isAdmin {
			provided := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
			if !strings.HasPrefix(r.Header.Get("Authorization"), "Bearer ") {
				provided = ""
			}
			hash := sha256.Sum256([]byte(provided))
			if subtle.ConstantTimeCompare(key[:], hash[:]) != 1 {
				w.Header().Set("WWW-Authenticate", "Bearer")
				http.Error(w, "unauthorized", 401)
				return
			}
			if r.URL.Path == "/api/local/shares" {
				if r.Method == http.MethodGet {
					w.Header().Set("Content-Type", "application/json; charset=utf-8")
					_ = json.NewEncoder(w).Encode(struct {
						Shares []shareListItem `json:"shares"`
					}{registry.list()})
					return
				}
				if r.Method != http.MethodPost {
					w.Header().Set("Allow", "GET, POST")
					http.Error(w, "method not allowed", 405)
					return
				}
				controller := http.NewResponseController(w)
				_ = controller.SetReadDeadline(time.Now().Add(30 * time.Second))
				defer controller.SetReadDeadline(time.Time{})
				decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096))
				decoder.DisallowUnknownFields()
				var input struct {
					Path string `json:"path"`
					TTL  int64  `json:"ttl_seconds"`
				}
				if err := decoder.Decode(&input); err != nil {
					http.Error(w, "invalid request", 400)
					return
				}
				if err := decoder.Decode(&struct{}{}); err != io.EOF {
					http.Error(w, "invalid request", 400)
					return
				}
				if input.TTL < 60 || input.TTL > 7*24*60*60 {
					http.Error(w, "invalid expiry", 400)
					return
				}
				file, info, err := store.OpenFile(input.Path)
				if err != nil {
					http.Error(w, "file not found", 404)
					return
				}
				file.Close()
				expires := now().Add(time.Duration(input.TTL) * time.Second)
				id, err := registry.add(sharedFile{path: input.Path, info: info, expires: expires})
				if err != nil {
					http.Error(w, "share unavailable", 503)
					return
				}
				w.Header().Set("Content-Type", "application/json; charset=utf-8")
				w.WriteHeader(201)
				_ = json.NewEncoder(w).Encode(struct {
					URL     string `json:"url"`
					Expires int64  `json:"expires"`
				}{"/s/" + id, expires.Unix()})
				return
			}
			if r.Method != http.MethodDelete {
				w.Header().Set("Allow", "DELETE")
				http.Error(w, "method not allowed", 405)
				return
			}
			id := strings.TrimPrefix(r.URL.Path, "/api/local/shares/")
			if !validShareID(id) || !registry.revoke(id) {
				http.Error(w, "not found", 404)
				return
			}
			w.WriteHeader(204)
			return
		}
		if r.Method != http.MethodGet && r.Method != http.MethodHead {
			w.Header().Set("Allow", "GET, HEAD")
			http.Error(w, "method not allowed", 405)
			return
		}
		id := strings.TrimPrefix(r.URL.Path, "/s/")
		if !validShareID(id) {
			http.Error(w, "not found", 404)
			return
		}
		item, ok := registry.get(id)
		if !ok {
			http.Error(w, "not found", 404)
			return
		}
		file, info, err := store.OpenFile(item.path)
		if err != nil {
			http.Error(w, "not found", 404)
			return
		}
		defer file.Close()
		if !os.SameFile(info, item.info) {
			http.Error(w, "not found", 404)
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Header().Set("Content-Disposition", mime.FormatMediaType("attachment", map[string]string{"filename": path.Base(item.path)}))
		http.ServeContent(w, r, info.Name(), info.ModTime(), file)
	}), nil
}
func validShareID(id string) bool {
	if len(id) != 43 {
		return false
	}
	b, err := base64.RawURLEncoding.Strict().DecodeString(id)
	return err == nil && len(b) == 32
}
