package goserver

import (
	"crypto/sha256"
	"crypto/subtle"
	"encoding/json"
	"errors"
	"fmt"
	"mime"
	"net/http"
	"net/url"
	"path"
	"strings"
	"time"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

type localCapabilities struct {
	Create bool `json:"create"`
	Delete bool `json:"delete"`
	Move   bool `json:"move"`
	Share  bool `json:"share"`
}

// LocalHandler adds an experimental authenticated, read-only local API. The
// token is an opaque shared secret, not an AList session or authorization model.
// The caller owns store and must close it after shutting down the server.
func LocalHandler(store *localfs.Store, token string) (http.Handler, error) {
	return LocalHandlerWithWrite(store, token, false)
}

// LocalHandlerWithWrite optionally enables create-only uploads. Never enable
// writes unless the bearer token and storage root are both appropriately secured.
func LocalHandlerWithWrite(store *localfs.Store, token string, allowWrite bool) (http.Handler, error) {
	return LocalHandlerWithPermissions(store, token, allowWrite, false)
}

// LocalHandlerWithPermissions independently enables creates and nonrecursive deletes.
func LocalHandlerWithPermissions(store *localfs.Store, token string, allowWrite, allowDelete bool) (http.Handler, error) {
	return LocalHandlerWithMove(store, token, allowWrite, allowDelete, false)
}

// LocalHandlerWithMove additionally enables non-overwriting local moves.
func LocalHandlerWithMove(store *localfs.Store, token string, allowWrite, allowDelete, allowMove bool) (http.Handler, error) {
	return LocalHandlerWithFeatures(store, token, allowWrite, allowDelete, allowMove, false)
}

// LocalHandlerWithFeatures reports the enabled share capability to the UI.
// The caller must also wrap this handler with ShareHandler when enabling it.
func LocalHandlerWithFeatures(store *localfs.Store, token string, allowWrite, allowDelete, allowMove, allowShare bool) (http.Handler, error) {
	if store == nil || len(token) < 32 || len(token) > 4096 || strings.ContainsAny(token, "\r\n\x00") {
		return nil, errors.New("local API requires a store and a token of 32–4096 bytes without controls")
	}
	key := sha256.Sum256([]byte(token))
	fallback := Handler()
	const maxUpload = int64(64 << 20)
	uploadSlots := make(chan struct{}, 8)
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if serveUI(w, r) {
			return
		}
		if r.URL.Path != "/api/local/list" && r.URL.Path != "/api/local/file" && r.URL.Path != "/api/local/mkdir" && r.URL.Path != "/api/local/move" {
			fallback.ServeHTTP(w, r)
			return
		}
		w.Header().Set("Cache-Control", "private, no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Content-Security-Policy", "default-src 'none'; sandbox")
		auth := r.Header.Get("Authorization")
		provided := ""
		if strings.HasPrefix(auth, "Bearer ") {
			provided = strings.TrimPrefix(auth, "Bearer ")
		}
		hash := sha256.Sum256([]byte(provided))
		if subtle.ConstantTimeCompare(key[:], hash[:]) != 1 {
			w.Header().Set("WWW-Authenticate", "Bearer")
			http.Error(w, "unauthorized", http.StatusUnauthorized)
			return
		}
		isUpload := allowWrite && r.URL.Path == "/api/local/file" && r.Method == http.MethodPut
		isMkdir := allowWrite && r.URL.Path == "/api/local/mkdir" && r.Method == http.MethodPost
		isDelete := allowDelete && r.URL.Path == "/api/local/file" && r.Method == http.MethodDelete
		isMove := allowMove && localfs.MoveSupported() && r.URL.Path == "/api/local/move" && r.Method == http.MethodPost
		if (r.URL.Path == "/api/local/mkdir" && !isMkdir) || (r.URL.Path == "/api/local/move" && !isMove) || (r.Method != http.MethodGet && r.Method != http.MethodHead && !isUpload && !isMkdir && !isDelete && !isMove) {
			allow := "GET, HEAD"
			if r.URL.Path == "/api/local/mkdir" {
				allow = ""
				if allowWrite {
					allow = "POST"
				}
			}
			if r.URL.Path == "/api/local/move" {
				allow = ""
				if allowMove && localfs.MoveSupported() {
					allow = "POST"
				}
			}
			if allowWrite && r.URL.Path == "/api/local/file" {
				allow += ", PUT"
			}
			if allowDelete && r.URL.Path == "/api/local/file" {
				allow += ", DELETE"
			}
			w.Header().Set("Allow", allow)
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		query, err := url.ParseQuery(r.URL.RawQuery)
		if err != nil {
			http.Error(w, "invalid query", http.StatusBadRequest)
			return
		}
		if len(query["path"]) > 1 {
			http.Error(w, "invalid path", http.StatusBadRequest)
			return
		}
		p := query.Get("path")
		if p == "" {
			p = "/"
		}
		if isMove {
			if r.ContentLength != 0 {
				http.Error(w, "request body not supported", http.StatusUnsupportedMediaType)
				return
			}
			if len(query["destination"]) != 1 || query.Get("destination") == "" {
				http.Error(w, "invalid destination", http.StatusBadRequest)
				return
			}
			if r.Header.Get("If") != "" || r.Header.Get("If-Match") != "" || r.Header.Get("If-None-Match") != "" || r.Header.Get("If-Unmodified-Since") != "" {
				http.Error(w, "unsupported precondition", http.StatusPreconditionFailed)
				return
			}
			if err := store.MoveNew(p, query.Get("destination")); err != nil {
				switch {
				case errors.Is(err, localfs.ErrInvalidPath):
					http.Error(w, "invalid path", http.StatusBadRequest)
				case errors.Is(err, localfs.ErrAlreadyExists):
					http.Error(w, "destination exists", http.StatusConflict)
				case errors.Is(err, localfs.ErrMoveUnsupported):
					http.Error(w, "atomic move unsupported", http.StatusNotImplemented)
				default:
					http.Error(w, "move failed", http.StatusConflict)
				}
				return
			}
			w.WriteHeader(http.StatusNoContent)
			return
		}
		if isDelete {
			if r.Header.Get("If-Match") != "" || r.Header.Get("If-None-Match") != "" || r.Header.Get("If-Unmodified-Since") != "" || r.Header.Get("If") != "" {
				http.Error(w, "unsupported precondition", http.StatusPreconditionFailed)
				return
			}
			if r.ContentLength != 0 {
				http.Error(w, "request body not supported", http.StatusUnsupportedMediaType)
				return
			}
			if err := store.Delete(p); err != nil {
				localDeleteError(w, err)
				return
			}
			w.WriteHeader(http.StatusNoContent)
			return
		}
		if isMkdir {
			if r.ContentLength != 0 {
				http.Error(w, "request body not supported", http.StatusUnsupportedMediaType)
				return
			}
			if err := store.Mkdir(p); err != nil {
				if errors.Is(err, localfs.ErrInvalidPath) {
					http.Error(w, "invalid path", http.StatusBadRequest)
				} else if errors.Is(err, localfs.ErrAlreadyExists) {
					http.Error(w, "destination exists", http.StatusConflict)
				} else {
					http.Error(w, "cannot create directory", http.StatusConflict)
				}
				return
			}
			w.WriteHeader(http.StatusCreated)
			return
		}
		if isUpload {
			if r.ContentLength > maxUpload {
				http.Error(w, "upload too large", http.StatusRequestEntityTooLarge)
				return
			}
			select {
			case uploadSlots <- struct{}{}:
				defer func() { <-uploadSlots }()
			default:
				http.Error(w, "too many uploads", http.StatusTooManyRequests)
				return
			}
			controller := http.NewResponseController(w)
			_ = controller.SetReadDeadline(time.Now().Add(5 * time.Minute))
			defer controller.SetReadDeadline(time.Time{})
			n, err := store.PutNew(r.Context(), p, r.Body, maxUpload)
			if err != nil {
				switch {
				case errors.Is(err, localfs.ErrInvalidPath):
					http.Error(w, "invalid path", http.StatusBadRequest)
				case errors.Is(err, localfs.ErrTooLarge):
					http.Error(w, "upload too large", http.StatusRequestEntityTooLarge)
				case errors.Is(err, localfs.ErrAlreadyExists):
					http.Error(w, "destination exists", http.StatusConflict)
				default:
					http.Error(w, "upload failed", http.StatusInternalServerError)
				}
				return
			}
			w.Header().Set("Content-Type", "application/json; charset=utf-8")
			w.WriteHeader(http.StatusCreated)
			_ = json.NewEncoder(w).Encode(struct {
				Size int64 `json:"size"`
			}{n})
			return
		}
		if r.URL.Path == "/api/local/list" {
			entries, err := store.List(p)
			if err != nil {
				localError(w, err)
				return
			}
			w.Header().Set("Content-Type", "application/json; charset=utf-8")
			if r.Method == http.MethodHead {
				return
			}
			_ = json.NewEncoder(w).Encode(struct {
				Entries      []localfs.Entry   `json:"entries"`
				Capabilities localCapabilities `json:"capabilities"`
			}{Entries: entries, Capabilities: localCapabilities{Create: allowWrite, Delete: allowDelete, Move: allowMove && localfs.MoveSupported(), Share: allowShare}})
			return
		}
		f, info, err := store.OpenFile(p)
		if err != nil {
			localError(w, err)
			return
		}
		defer f.Close()
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Header().Set("Content-Disposition", mime.FormatMediaType("attachment", map[string]string{"filename": path.Base(p)}))
		http.ServeContent(w, r, info.Name(), info.ModTime(), f)
	}), nil
}

func localError(w http.ResponseWriter, err error) {
	if errors.Is(err, localfs.ErrInvalidPath) {
		http.Error(w, "invalid path", http.StatusBadRequest)
		return
	}
	if errors.Is(err, localfs.ErrTooManyEntries) {
		http.Error(w, "directory too large", http.StatusRequestEntityTooLarge)
		return
	}
	// Avoid leaking details about paths, permissions, or symlink resolution.
	http.Error(w, "not found", http.StatusNotFound)
}

func localDeleteError(w http.ResponseWriter, err error) {
	if errors.Is(err, localfs.ErrDirectoryNotEmpty) {
		http.Error(w, "directory not empty", http.StatusConflict)
		return
	}
	localError(w, err)
}

// ConfigureLocal attaches the local API to a new server.
func ConfigureLocal(server *http.Server, store *localfs.Store, token string) error {
	return ConfigureLocalWithWrite(server, store, token, false)
}

// ConfigureLocalWithWrite attaches the opt-in create-only local API.
func ConfigureLocalWithWrite(server *http.Server, store *localfs.Store, token string, allowWrite bool) error {
	return ConfigureLocalWithPermissions(server, store, token, allowWrite, false)
}

// ConfigureLocalWithPermissions enables the requested operations on the local API.
func ConfigureLocalWithPermissions(server *http.Server, store *localfs.Store, token string, allowWrite, allowDelete bool) error {
	return ConfigureLocalWithMove(server, store, token, allowWrite, allowDelete, false)
}

// ConfigureLocalWithMove attaches an API with optional move support.
func ConfigureLocalWithMove(server *http.Server, store *localfs.Store, token string, allowWrite, allowDelete, allowMove bool) error {
	return ConfigureLocalWithFeatures(server, store, token, allowWrite, allowDelete, allowMove, false)
}

// ConfigureLocalWithFeatures attaches the local API and optional share handler.
func ConfigureLocalWithFeatures(server *http.Server, store *localfs.Store, token string, allowWrite, allowDelete, allowMove, allowShare bool) error {
	if server == nil {
		return fmt.Errorf("nil server")
	}
	handler, err := LocalHandlerWithFeatures(store, token, allowWrite, allowDelete, allowMove, allowShare)
	if err != nil {
		return err
	}
	if allowShare {
		handler, err = ShareHandler(store, token, handler)
		if err != nil {
			return err
		}
	}
	server.Handler = handler
	return nil
}
