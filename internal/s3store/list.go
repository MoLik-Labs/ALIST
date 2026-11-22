package s3store

import (
	"context"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

// Entry describes an immediate object or virtual directory in a listing.
type Entry struct {
	Name  string
	IsDir bool
	Size  int64
	ETag  string
}

// Page is one ListObjectsV2 response. NextToken is opaque and must be passed
// back unchanged; an empty token means the listing is complete.
type Page struct {
	Entries   []Entry
	NextToken string
}

// ListPage lists at most 1000 immediate children of prefix. Prefix is empty
// for the root or a slash-terminated object-key prefix. The caller controls
// pagination and must impose its own limit on the number of pages consumed.
func (c *Client) ListPage(ctx context.Context, prefix, token string) (Page, error) {
	if ctx == nil {
		return Page{}, errors.New("nil context")
	}
	if prefix != "" && (!strings.HasSuffix(prefix, "/") || !validKey(strings.TrimSuffix(prefix, "/"))) {
		return Page{}, errors.New("invalid prefix")
	}
	if len(token) > 8192 {
		return Page{}, errors.New("invalid continuation token")
	}
	params := map[string]string{"list-type": "2", "delimiter": "/", "max-keys": "1000", "prefix": prefix, "encoding-type": "url"}
	if token != "" {
		params["continuation-token"] = token
	}
	signed, err := c.presign("", params, time.Now(), 60)
	if err != nil {
		return Page{}, ErrRequestFailed
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, signed, nil)
	if err != nil {
		return Page{}, ErrRequestFailed
	}
	resp, err := c.http.Do(req)
	if err != nil {
		if ctx.Err() != nil {
			return Page{}, ctx.Err()
		}
		return Page{}, ErrRequestFailed
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return Page{}, fmt.Errorf("%w: HTTP %d", ErrRequestFailed, resp.StatusCode)
	}
	const maxXML = 4 << 20
	data, err := io.ReadAll(io.LimitReader(resp.Body, maxXML+1))
	if err != nil || len(data) > maxXML {
		return Page{}, ErrRequestFailed
	}
	var result struct {
		XMLName   xml.Name `xml:"ListBucketResult"`
		Encoding  string   `xml:"EncodingType"`
		Truncated bool     `xml:"IsTruncated"`
		Next      string   `xml:"NextContinuationToken"`
		Contents  []struct {
			Key  string `xml:"Key"`
			Size int64  `xml:"Size"`
			ETag string `xml:"ETag"`
		} `xml:"Contents"`
		Prefixes []struct {
			Prefix string `xml:"Prefix"`
		} `xml:"CommonPrefixes"`
	}
	if xml.Unmarshal(data, &result) != nil || result.Encoding != "url" || len(result.Contents)+len(result.Prefixes) > 1000 || len(result.Next) > 8192 || (result.Truncated && (result.Next == "" || result.Next == token)) || (!result.Truncated && result.Next != "") {
		return Page{}, ErrRequestFailed
	}
	page := Page{}
	if result.Truncated {
		page.NextToken = result.Next
	}
	for _, item := range result.Prefixes {
		key, err := url.PathUnescape(item.Prefix)
		if err != nil || !strings.HasPrefix(key, prefix) || !strings.HasSuffix(key, "/") {
			return Page{}, ErrRequestFailed
		}
		name := strings.TrimSuffix(strings.TrimPrefix(key, prefix), "/")
		if !validKey(name) || strings.Contains(name, "/") {
			return Page{}, ErrRequestFailed
		}
		page.Entries = append(page.Entries, Entry{Name: name, IsDir: true})
	}
	for _, item := range result.Contents {
		key, err := url.PathUnescape(item.Key)
		if err != nil || !strings.HasPrefix(key, prefix) || item.Size < 0 {
			return Page{}, ErrRequestFailed
		}
		if key == prefix {
			continue
		} // directory marker
		name := strings.TrimPrefix(key, prefix)
		if !validKey(name) || strings.Contains(name, "/") {
			return Page{}, ErrRequestFailed
		}
		page.Entries = append(page.Entries, Entry{Name: name, Size: item.Size, ETag: item.ETag})
	}
	return page, nil
}
