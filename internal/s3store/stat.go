package s3store

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"
)

// ErrNotFound indicates that the object endpoint returned HTTP 404. Providers
// may instead return 403 for a missing object when listing is not permitted.
var ErrNotFound = errors.New("object not found")

// ObjectInfo contains the metadata returned by a HEAD request. LastModified
// is zero when the provider did not supply that header. ETag is opaque and
// must not be interpreted as a content hash.
type ObjectInfo struct {
	Size         int64
	ETag         string
	LastModified time.Time
	ContentType  string
}

// Stat retrieves metadata for an object without downloading its content. It
// does not infer the existence of virtual directories from a prefix listing.
func (c *Client) Stat(ctx context.Context, key string) (ObjectInfo, error) {
	if ctx == nil {
		return ObjectInfo{}, errors.New("nil context")
	}
	if !validKey(key) {
		return ObjectInfo{}, errors.New("invalid object request")
	}
	signed, err := c.presign(http.MethodHead, key, nil, time.Now(), 60)
	if err != nil {
		return ObjectInfo{}, ErrRequestFailed
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodHead, signed, nil)
	if err != nil {
		return ObjectInfo{}, ErrRequestFailed
	}
	resp, err := c.http.Do(req)
	if err != nil {
		if ctx.Err() != nil {
			return ObjectInfo{}, ctx.Err()
		}
		return ObjectInfo{}, ErrRequestFailed
	}
	defer resp.Body.Close()
	if resp.StatusCode == http.StatusNotFound {
		return ObjectInfo{}, ErrNotFound
	}
	if resp.StatusCode != http.StatusOK {
		return ObjectInfo{}, fmt.Errorf("%w: HTTP %d", ErrRequestFailed, resp.StatusCode)
	}
	if resp.ContentLength < 0 {
		return ObjectInfo{}, ErrRequestFailed
	}
	info := ObjectInfo{Size: resp.ContentLength, ETag: resp.Header.Get("ETag"), ContentType: resp.Header.Get("Content-Type")}
	if modified := resp.Header.Get("Last-Modified"); modified != "" {
		info.LastModified, err = http.ParseTime(modified)
		if err != nil {
			return ObjectInfo{}, ErrRequestFailed
		}
	}
	return info, nil
}
