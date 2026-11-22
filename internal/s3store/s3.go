// Package s3store implements an experimental, read-only S3-compatible object
// reader. It is not yet connected to Kova's VFS or a production server.
package s3store

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"path"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/MoLik-Labs/ALIST/internal/outbound"
)

var ErrRequestFailed = errors.New("object storage request failed")

type Config struct {
	// BucketURL identifies the bucket, for example https://s3.example.com/bucket
	// or https://bucket.s3.example.com. It must not contain credentials or query.
	BucketURL, Region, AccessKey, SecretKey, SessionToken string
	// Private and plain HTTP endpoints require separate explicit opt-ins.
	AllowPrivate, AllowInsecureHTTP bool
}

type Client struct {
	endpoint                                   *url.URL
	region, accessKey, secretKey, sessionToken string
	http                                       *outbound.Client
}

func New(cfg Config) (*Client, error) {
	u, err := url.Parse(cfg.BucketURL)
	if err != nil || u == nil || u.Host == "" || u.Hostname() == "" || u.User != nil || u.Opaque != "" || u.RawQuery != "" || u.ForceQuery || u.Fragment != "" || (u.Scheme != "https" && !(u.Scheme == "http" && cfg.AllowInsecureHTTP)) {
		return nil, errors.New("invalid bucket URL")
	}
	if len(cfg.BucketURL) > 2048 || strings.ContainsAny(cfg.BucketURL, "\r\n\t ") || strings.Contains(u.EscapedPath(), "%") || (u.Path != "" && path.Clean(u.Path) != u.Path) || strings.Contains(u.Hostname(), "%") {
		return nil, errors.New("invalid bucket URL")
	}
	if !validRegion(cfg.Region) || !validCredential(cfg.AccessKey, 256) || !validCredential(cfg.SecretKey, 4096) || (cfg.SessionToken != "" && !validCredential(cfg.SessionToken, 8192)) {
		return nil, errors.New("invalid S3 credentials or region")
	}
	u.Host = strings.ToLower(u.Host)
	return &Client{endpoint: u, region: cfg.Region, accessKey: cfg.AccessKey, secretKey: cfg.SecretKey, sessionToken: cfg.SessionToken, http: outbound.New(outbound.Options{AllowPrivate: cfg.AllowPrivate, Timeout: 10 * time.Minute})}, nil
}
func validRegion(s string) bool {
	if len(s) < 1 || len(s) > 128 {
		return false
	}
	for _, c := range s {
		if !(c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '-') {
			return false
		}
	}
	return true
}
func validCredential(s string, max int) bool {
	if len(s) == 0 || len(s) > max || !utf8.ValidString(s) {
		return false
	}
	for _, c := range s {
		if c <= 0x20 || c == 0x7f {
			return false
		}
	}
	return true
}
func validKey(key string) bool {
	if len(key) < 1 || len(key) > 1024 || !utf8.ValidString(key) || strings.HasPrefix(key, "/") {
		return false
	}
	for _, r := range key {
		if r < 0x20 || r == 0x7f || r == '\\' {
			return false
		}
	}
	for _, part := range strings.Split(key, "/") {
		if part == "" || part == "." || part == ".." {
			return false
		}
	}
	return true
}

// awsEscape encodes UTF-8 bytes according to SigV4 RFC 3986 rules.
func awsEscape(s string, slash bool) string {
	const digits = "0123456789ABCDEF"
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || strings.ContainsRune("-_.~", rune(c)) || (slash && c == '/') {
			b.WriteByte(c)
		} else {
			b.WriteByte('%')
			b.WriteByte(digits[c>>4])
			b.WriteByte(digits[c&15])
		}
	}
	return b.String()
}
func hmacSHA256(key []byte, value string) []byte {
	h := hmac.New(sha256.New, key)
	_, _ = h.Write([]byte(value))
	return h.Sum(nil)
}
func (c *Client) presignGet(key string, at time.Time, ttl int) (string, error) {
	if !validKey(key) || ttl < 1 || ttl > 604800 || at.IsZero() {
		return "", errors.New("invalid object request")
	}
	at = at.UTC()
	day := at.Format("20060102")
	stamp := at.Format("20060102T150405Z")
	scope := day + "/" + c.region + "/s3/aws4_request"
	uri := strings.TrimSuffix(c.endpoint.Path, "/") + "/" + key
	encodedPath := awsEscape(uri, true)
	params := map[string]string{"X-Amz-Algorithm": "AWS4-HMAC-SHA256", "X-Amz-Credential": c.accessKey + "/" + scope, "X-Amz-Date": stamp, "X-Amz-Expires": strconv.Itoa(ttl), "X-Amz-SignedHeaders": "host"}
	if c.sessionToken != "" {
		params["X-Amz-Security-Token"] = c.sessionToken
	}
	names := make([]string, 0, len(params))
	for k := range params {
		names = append(names, k)
	}
	sort.Strings(names)
	parts := make([]string, 0, len(names))
	for _, k := range names {
		parts = append(parts, awsEscape(k, false)+"="+awsEscape(params[k], false))
	}
	query := strings.Join(parts, "&")
	canonical := "GET\n" + encodedPath + "\n" + query + "\nhost:" + c.endpoint.Host + "\n\nhost\nUNSIGNED-PAYLOAD"
	digest := sha256.Sum256([]byte(canonical))
	stringToSign := "AWS4-HMAC-SHA256\n" + stamp + "\n" + scope + "\n" + hex.EncodeToString(digest[:])
	dateKey := hmacSHA256([]byte("AWS4"+c.secretKey), day)
	regionKey := hmacSHA256(dateKey, c.region)
	serviceKey := hmacSHA256(regionKey, "s3")
	signingKey := hmacSHA256(serviceKey, "aws4_request")
	signature := hex.EncodeToString(hmacSHA256(signingKey, stringToSign))
	return c.endpoint.Scheme + "://" + c.endpoint.Host + encodedPath + "?" + query + "&X-Amz-Signature=" + signature, nil
}

// Open streams a single object. The caller must close the response body and
// bound its own read size; errors do not include presigned URLs or credentials.
func (c *Client) Open(ctx context.Context, key string) (io.ReadCloser, error) {
	if ctx == nil {
		return nil, errors.New("nil context")
	}
	signed, err := c.presignGet(key, time.Now(), 60)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, signed, nil)
	if err != nil {
		return nil, ErrRequestFailed
	}
	response, err := c.http.Do(req)
	if err != nil {
		if ctx.Err() != nil {
			return nil, ctx.Err()
		}
		return nil, ErrRequestFailed
	}
	if response.StatusCode != http.StatusOK {
		response.Body.Close()
		return nil, fmt.Errorf("%w: HTTP %d", ErrRequestFailed, response.StatusCode)
	}
	return response.Body, nil
}
func (c *Client) Close() { c.http.CloseIdleConnections() }
