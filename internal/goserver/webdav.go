package goserver

import (
	"bytes"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"mime"
	"net/http"
	"net/url"
	"path"
	"strings"
	"syscall"
	"time"

	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

const davNS = "DAV:"

type davRequest struct {
	XMLName xml.Name  `xml:"DAV: propfind"`
	All     *struct{} `xml:"DAV: allprop"`
	Names   *struct{} `xml:"DAV: propname"`
	Prop    *struct {
		Items []struct{ XMLName xml.Name } `xml:",any"`
	} `xml:"DAV: prop"`
}

var davProperties = []xml.Name{{Space: davNS, Local: "displayname"}, {Space: davNS, Local: "resourcetype"}, {Space: davNS, Local: "getcontentlength"}, {Space: davNS, Local: "getlastmodified"}, {Space: davNS, Local: "getcontenttype"}}

func davFindProperties(r *http.Request) ([]xml.Name, bool, error) {
	body, err := io.ReadAll(io.LimitReader(r.Body, 65537))
	if err != nil {
		return nil, false, err
	}
	if len(body) > 65536 {
		return nil, false, errors.New("request too large")
	}
	if len(bytes.TrimSpace(body)) == 0 {
		return davProperties, false, nil
	}
	var request davRequest
	decoder := xml.NewDecoder(bytes.NewReader(body))
	if err := decoder.Decode(&request); err != nil {
		return nil, false, err
	}
	for {
		token, err := decoder.Token()
		if errors.Is(err, io.EOF) {
			break
		}
		if err != nil {
			return nil, false, err
		}
		switch value := token.(type) {
		case xml.CharData:
			if len(bytes.TrimSpace(value)) != 0 {
				return nil, false, errors.New("trailing XML data")
			}
		case xml.Comment, xml.ProcInst:
		default:
			return nil, false, errors.New("trailing XML content")
		}
	}
	count := 0
	if request.All != nil {
		count++
	}
	if request.Names != nil {
		count++
	}
	if request.Prop != nil {
		count++
	}
	if count != 1 {
		return nil, false, errors.New("unsupported propfind")
	}
	if request.All != nil {
		return davProperties, false, nil
	}
	if request.Names != nil {
		return davProperties, true, nil
	}
	if len(request.Prop.Items) > 32 {
		return nil, false, errors.New("too many properties")
	}
	var properties []xml.Name
	for _, item := range request.Prop.Items {
		if len(item.XMLName.Space) > 256 || len(item.XMLName.Local) > 128 {
			return nil, false, errors.New("property name too long")
		}
		properties = append(properties, item.XMLName)
	}
	return properties, false, nil
}

type davItem struct {
	path  string
	entry localfs.Entry
}

var errDAVResponseTooLarge = errors.New("WebDAV response too large")

type boundedDAVBuffer struct{ bytes.Buffer }

func (b *boundedDAVBuffer) Write(p []byte) (int, error) {
	const maxResponseBytes = 4 << 20
	if len(p) > maxResponseBytes-b.Len() {
		return 0, errDAVResponseTooLarge
	}
	return b.Buffer.Write(p)
}

func davResponseXML(items []davItem, props []xml.Name, namesOnly bool) ([]byte, error) {
	var out boundedDAVBuffer
	_, _ = out.Write([]byte(xml.Header))
	enc := xml.NewEncoder(&out)
	start := func(name xml.Name) error { return enc.EncodeToken(xml.StartElement{Name: name}) }
	end := func(name xml.Name) error { return enc.EncodeToken(xml.EndElement{Name: name}) }
	text := func(name xml.Name, value string) error { return enc.EncodeElement(value, xml.StartElement{Name: name}) }
	dav := func(local string) xml.Name { return xml.Name{Space: davNS, Local: local} }
	if err := start(dav("multistatus")); err != nil {
		return nil, err
	}
	for _, item := range items {
		if err := start(dav("response")); err != nil {
			return nil, err
		}
		href := "/dav" + item.path
		if item.entry.IsDir && !strings.HasSuffix(href, "/") {
			href += "/"
		}
		u := url.URL{Path: href}
		if err := text(dav("href"), u.EscapedPath()); err != nil {
			return nil, err
		}
		var known, unknown []xml.Name
		for _, p := range props {
			found := false
			for _, supported := range davProperties {
				if p == supported {
					found = true
					break
				}
			}
			if found {
				known = append(known, p)
			} else {
				unknown = append(unknown, p)
			}
		}
		for groupIndex, group := range [][]xml.Name{known, unknown} {
			if len(group) == 0 {
				continue
			}
			if err := start(dav("propstat")); err != nil {
				return nil, err
			}
			if err := start(dav("prop")); err != nil {
				return nil, err
			}
			for _, property := range group {
				if err := start(property); err != nil {
					return nil, err
				}
				if groupIndex == 0 && !namesOnly {
					switch property.Local {
					case "displayname":
						name := item.entry.Name
						if item.path == "/" {
							name = "root"
						}
						if err := enc.EncodeToken(xml.CharData(name)); err != nil {
							return nil, err
						}
					case "resourcetype":
						if item.entry.IsDir {
							if err := start(dav("collection")); err != nil {
								return nil, err
							}
							if err := end(dav("collection")); err != nil {
								return nil, err
							}
						}
					case "getcontentlength":
						if !item.entry.IsDir {
							if err := enc.EncodeToken(xml.CharData(fmt.Sprint(item.entry.Size))); err != nil {
								return nil, err
							}
						}
					case "getlastmodified":
						if err := enc.EncodeToken(xml.CharData(time.Unix(item.entry.Modified, 0).UTC().Format(http.TimeFormat))); err != nil {
							return nil, err
						}
					case "getcontenttype":
						contentType := "application/octet-stream"
						if item.entry.IsDir {
							contentType = "httpd/unix-directory"
						}
						if err := enc.EncodeToken(xml.CharData(contentType)); err != nil {
							return nil, err
						}
					}
				}
				if err := end(property); err != nil {
					return nil, err
				}
			}
			if err := end(dav("prop")); err != nil {
				return nil, err
			}
			status := "HTTP/1.1 200 OK"
			if groupIndex == 1 {
				status = "HTTP/1.1 404 Not Found"
			}
			if err := text(dav("status"), status); err != nil {
				return nil, err
			}
			if err := end(dav("propstat")); err != nil {
				return nil, err
			}
		}
		if err := end(dav("response")); err != nil {
			return nil, err
		}
	}
	if err := end(dav("multistatus")); err != nil {
		return nil, err
	}
	if err := enc.Flush(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}

// WebDAVHandler exposes the read-only subset by default.
func WebDAVHandler(store *localfs.Store, token string, next http.Handler) (http.Handler, error) {
	return WebDAVHandlerWithWrite(store, token, next, false)
}

// WebDAVHandlerWithWrite optionally enables MKCOL and create-only PUT. This is
// not full RFC 4918 support; it requires TLS or loopback for Basic credentials.
func WebDAVHandlerWithWrite(store *localfs.Store, token string, next http.Handler, allowWrite bool) (http.Handler, error) {
	return WebDAVHandlerWithPermissions(store, token, next, allowWrite, false)
}

// WebDAVHandlerWithPermissions optionally enables nonrecursive deletion.
func WebDAVHandlerWithPermissions(store *localfs.Store, token string, next http.Handler, allowWrite, allowDelete bool) (http.Handler, error) {
	return WebDAVHandlerWithMove(store, token, next, allowWrite, allowDelete, false)
}

// WebDAVHandlerWithMove additionally enables atomic non-overwriting MOVE.
func WebDAVHandlerWithMove(store *localfs.Store, token string, next http.Handler, allowWrite, allowDelete, allowMove bool) (http.Handler, error) {
	if store == nil || next == nil || len(token) < 32 || len(token) > 4096 || strings.ContainsAny(token, "\r\n\x00") {
		return nil, errors.New("invalid WebDAV configuration")
	}
	key := sha256.Sum256([]byte(token))
	const maxUpload = int64(64 << 20)
	uploadSlots := make(chan struct{}, 8)
	propfindSlots := make(chan struct{}, 8)
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/dav" && !strings.HasPrefix(r.URL.Path, "/dav/") {
			next.ServeHTTP(w, r)
			return
		}
		w.Header().Set("Cache-Control", "private, no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("Content-Security-Policy", "default-src 'none'; sandbox")
		allow := "OPTIONS, PROPFIND, GET, HEAD"
		if allowWrite {
			allow += ", PUT, MKCOL"
		}
		if allowDelete {
			allow += ", DELETE"
		}
		if allowMove && localfs.MoveSupported() {
			allow += ", MOVE"
		}
		w.Header().Set("Allow", allow)
		provided := ""
		if strings.HasPrefix(r.Header.Get("Authorization"), "Bearer ") {
			provided = strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
		}
		if user, password, ok := r.BasicAuth(); ok && user == "kova" {
			provided = password
		}
		hash := sha256.Sum256([]byte(provided))
		if subtle.ConstantTimeCompare(key[:], hash[:]) != 1 {
			w.Header().Set("WWW-Authenticate", `Basic realm="Kova", charset="UTF-8"`)
			http.Error(w, "unauthorized", http.StatusUnauthorized)
			return
		}
		p := strings.TrimPrefix(r.URL.Path, "/dav")
		if p == "" {
			p = "/"
		}
		// Clients commonly use a trailing slash for collection URLs.
		if p != "/" && strings.HasSuffix(p, "/") {
			p = strings.TrimSuffix(p, "/")
		}
		switch r.Method {
		case http.MethodOptions:
			w.WriteHeader(http.StatusNoContent)
			return
		case http.MethodGet, http.MethodHead:
			f, info, err := store.OpenFile(p)
			if err != nil {
				localError(w, err)
				return
			}
			defer f.Close()
			w.Header().Set("Content-Type", "application/octet-stream")
			w.Header().Set("Content-Disposition", mime.FormatMediaType("attachment", map[string]string{"filename": path.Base(p)}))
			http.ServeContent(w, r, info.Name(), info.ModTime(), f)
			return
		case "MOVE":
			if !allowMove || !localfs.MoveSupported() {
				http.Error(w, "WebDAV MOVE not enabled", http.StatusMethodNotAllowed)
				return
			}
			if r.ContentLength != 0 {
				http.Error(w, "request body not supported", http.StatusUnsupportedMediaType)
				return
			}
			if r.Header.Get("If") != "" || r.Header.Get("If-Match") != "" || r.Header.Get("If-None-Match") != "" || r.Header.Get("If-Unmodified-Since") != "" {
				http.Error(w, "unsupported precondition", http.StatusPreconditionFailed)
				return
			}
			if depth := r.Header.Get("Depth"); depth != "" && depth != "infinity" {
				http.Error(w, "invalid MOVE depth", http.StatusBadRequest)
				return
			}
			values := r.Header.Values("Overwrite")
			if len(values) > 1 || (len(values) == 1 && values[0] != "T" && values[0] != "F") {
				http.Error(w, "invalid Overwrite", http.StatusBadRequest)
				return
			}
			destination, err := davMoveDestination(r)
			if err != nil {
				http.Error(w, "invalid Destination", http.StatusBadRequest)
				return
			}
			if err := store.MoveNew(p, destination); err != nil {
				switch {
				case errors.Is(err, localfs.ErrInvalidPath):
					http.Error(w, "invalid path", http.StatusBadRequest)
				case errors.Is(err, localfs.ErrAlreadyExists):
					status := http.StatusForbidden
					if len(values) == 1 && values[0] == "F" {
						status = http.StatusPreconditionFailed
					}
					http.Error(w, "destination exists; overwrite not supported", status)
				case errors.Is(err, localfs.ErrMoveUnsupported):
					http.Error(w, "atomic move unsupported", http.StatusNotImplemented)
				default:
					http.Error(w, "move failed", http.StatusConflict)
				}
				return
			}
			w.WriteHeader(http.StatusCreated)
			return
		case http.MethodDelete:
			if !allowDelete {
				http.Error(w, "WebDAV deletion not enabled", http.StatusMethodNotAllowed)
				return
			}
			if r.Header.Get("If") != "" || r.Header.Get("If-Match") != "" || r.Header.Get("If-None-Match") != "" || r.Header.Get("If-Unmodified-Since") != "" {
				http.Error(w, "unsupported precondition", http.StatusPreconditionFailed)
				return
			}
			if r.ContentLength != 0 {
				http.Error(w, "request body not supported", http.StatusUnsupportedMediaType)
				return
			}
			if depth := r.Header.Get("Depth"); depth != "" && depth != "infinity" {
				http.Error(w, "invalid DELETE depth", http.StatusBadRequest)
				return
			}
			if err := store.Delete(p); err != nil {
				localDeleteError(w, err)
				return
			}
			w.WriteHeader(http.StatusNoContent)
			return
		case http.MethodPut, "MKCOL":
			if !allowWrite {
				http.Error(w, "WebDAV method not enabled", http.StatusMethodNotAllowed)
				return
			}
			// Locks are not implemented. Refuse conditional writes rather than
			// silently ignoring conditions or accidentally replacing content.
			if r.Header.Get("If") != "" || r.Header.Get("If-Match") != "" || r.Header.Get("If-Unmodified-Since") != "" || (r.Header.Get("If-None-Match") != "" && r.Header.Get("If-None-Match") != "*") {
				http.Error(w, "unsupported precondition", http.StatusPreconditionFailed)
				return
			}
			if r.Method == "MKCOL" {
				controller := http.NewResponseController(w)
				_ = controller.SetReadDeadline(time.Now().Add(30 * time.Second))
				body, err := io.ReadAll(io.LimitReader(r.Body, 1))
				_ = controller.SetReadDeadline(time.Time{})
				if err != nil {
					http.Error(w, "invalid body", http.StatusBadRequest)
					return
				}
				if len(body) != 0 {
					http.Error(w, "MKCOL body is not supported", http.StatusUnsupportedMediaType)
					return
				}
				err = store.Mkdir(p)
				if err != nil {
					davWriteError(w, r, err, true)
					return
				}
				w.WriteHeader(http.StatusCreated)
				return
			}
			if strings.HasSuffix(r.URL.Path, "/") {
				http.Error(w, "PUT requires a file path", http.StatusMethodNotAllowed)
				return
			}
			if r.Header.Get("Content-Range") != "" {
				http.Error(w, "partial PUT not supported", http.StatusNotImplemented)
				return
			}
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
			_, err := store.PutNew(r.Context(), p, r.Body, maxUpload)
			if err != nil {
				davWriteError(w, r, err, false)
				return
			}
			w.WriteHeader(http.StatusCreated)
			return
		case "PROPFIND":
			select {
			case propfindSlots <- struct{}{}:
				defer func() { <-propfindSlots }()
			default:
				http.Error(w, "too many PROPFIND requests", http.StatusTooManyRequests)
				return
			}
			depth := strings.TrimSpace(r.Header.Get("Depth"))
			if depth != "0" && depth != "1" {
				http.Error(w, "only Depth 0 or 1 is supported", http.StatusForbidden)
				return
			}
			controller := http.NewResponseController(w)
			_ = controller.SetReadDeadline(time.Now().Add(30 * time.Second))
			props, namesOnly, err := davFindProperties(r)
			_ = controller.SetReadDeadline(time.Time{})
			if err != nil {
				http.Error(w, "invalid PROPFIND", http.StatusBadRequest)
				return
			}
			entry, err := store.Stat(p)
			if err != nil {
				localError(w, err)
				return
			}
			items := []davItem{{p, entry}}
			if depth == "1" && entry.IsDir {
				children, err := store.ListLimit(p, 1000)
				if err != nil {
					if errors.Is(err, localfs.ErrTooManyEntries) {
						http.Error(w, "directory exceeds WebDAV listing limit", http.StatusInsufficientStorage)
					} else {
						localError(w, err)
					}
					return
				}
				for _, child := range children {
					childPath := strings.TrimSuffix(p, "/") + "/" + child.Name
					items = append(items, davItem{childPath, child})
				}
			}
			body, err := davResponseXML(items, props, namesOnly)
			if err != nil {
				if errors.Is(err, errDAVResponseTooLarge) {
					http.Error(w, "WebDAV response exceeds limit", http.StatusInsufficientStorage)
				} else {
					http.Error(w, "invalid properties", http.StatusBadRequest)
				}
				return
			}
			w.Header().Set("Content-Type", "application/xml; charset=utf-8")
			w.WriteHeader(http.StatusMultiStatus)
			_, _ = w.Write(body)
			return
		default:
			http.Error(w, "WebDAV method not implemented", http.StatusMethodNotAllowed)
		}
	}), nil
}

func davWriteError(w http.ResponseWriter, r *http.Request, err error, collection bool) {
	switch {
	case errors.Is(err, localfs.ErrInvalidPath):
		http.Error(w, "invalid path", http.StatusBadRequest)
	case errors.Is(err, localfs.ErrTooLarge):
		http.Error(w, "upload too large", http.StatusRequestEntityTooLarge)
	case errors.Is(err, localfs.ErrAlreadyExists):
		status := http.StatusConflict
		if collection {
			status = http.StatusMethodNotAllowed
		}
		if r.Header.Get("If-None-Match") == "*" {
			status = http.StatusPreconditionFailed
		}
		http.Error(w, "destination exists", status)
	case errors.Is(err, syscall.ENOENT), errors.Is(err, syscall.ENOTDIR):
		http.Error(w, "parent collection missing", http.StatusConflict)
	case errors.Is(err, syscall.EACCES), errors.Is(err, syscall.EPERM), errors.Is(err, syscall.ELOOP):
		http.Error(w, "forbidden", http.StatusForbidden)
	default:
		http.Error(w, "write failed", http.StatusInternalServerError)
	}
}

// davMoveDestination accepts same-origin absolute URLs and root-relative paths.
// Forwarded headers are deliberately not trusted for determining the scheme.
func davMoveDestination(r *http.Request) (string, error) {
	values := r.Header.Values("Destination")
	if len(values) != 1 || values[0] == "" {
		return "", errors.New("missing destination")
	}
	u, err := url.Parse(values[0])
	if err != nil {
		return "", err
	}
	if u.User != nil || u.Opaque != "" || u.RawQuery != "" || u.ForceQuery || u.Fragment != "" {
		return "", errors.New("invalid destination URL")
	}
	if u.IsAbs() {
		scheme := "http"
		if r.TLS != nil {
			scheme = "https"
		}
		if u.Scheme != scheme || u.Host == "" || !strings.EqualFold(u.Host, r.Host) {
			return "", errors.New("cross-origin destination")
		}
	} else if u.Host != "" || !strings.HasPrefix(u.Path, "/") {
		return "", errors.New("invalid relative destination")
	}
	if u.Path != "/dav" && !strings.HasPrefix(u.Path, "/dav/") {
		return "", errors.New("outside WebDAV root")
	}
	p := strings.TrimPrefix(u.Path, "/dav")
	if p == "" {
		p = "/"
	}
	if p != "/" && strings.HasSuffix(p, "/") {
		p = strings.TrimSuffix(p, "/")
	}
	return p, nil
}
