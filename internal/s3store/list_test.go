package s3store

import (
	"context"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestListPagePagination(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		q := r.URL.Query()
		if r.URL.Path != "/bucket/" || q.Get("prefix") != "docs/" || q.Get("encoding-type") != "url" || q.Get("delimiter") != "/" || q.Get("X-Amz-Signature") == "" {
			t.Error("bad request")
			w.WriteHeader(400)
			return
		}
		if q.Get("continuation-token") == "" {
			io.WriteString(w, `<ListBucketResult><EncodingType>url</EncodingType><IsTruncated>true</IsTruncated><NextContinuationToken>a+/=</NextContinuationToken><CommonPrefixes><Prefix>docs%2Fsub%2F</Prefix></CommonPrefixes><Contents><Key>docs%2Fa%20b.txt</Key><Size>5</Size><ETag>etag</ETag></Contents></ListBucketResult>`)
			return
		}
		if q.Get("continuation-token") != "a+/=" {
			t.Error("bad token")
		}
		io.WriteString(w, `<ListBucketResult><EncodingType>url</EncodingType><IsTruncated>false</IsTruncated><Contents><Key>docs%2F</Key><Size>0</Size></Contents></ListBucketResult>`)
	}))
	defer server.Close()
	c, err := New(Config{BucketURL: server.URL + "/bucket", Region: "auto", AccessKey: "key", SecretKey: "secret", AllowPrivate: true, AllowInsecureHTTP: true})
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	p, err := c.ListPage(context.Background(), "docs/", "")
	if err != nil || len(p.Entries) != 2 || p.Entries[0].Name != "sub" || !p.Entries[0].IsDir || p.Entries[1].Name != "a b.txt" || p.Entries[1].Size != 5 || p.NextToken != "a+/=" {
		t.Fatalf("%+v %v", p, err)
	}
	p, err = c.ListPage(context.Background(), "docs/", p.NextToken)
	if err != nil || len(p.Entries) != 0 || p.NextToken != "" {
		t.Fatalf("%+v %v", p, err)
	}
}

func TestListPageRejectsInvalidResponses(t *testing.T) {
	for _, body := range []string{`<wrong/>`, `<ListBucketResult><EncodingType>url</EncodingType><IsTruncated>true</IsTruncated></ListBucketResult>`, `<ListBucketResult><EncodingType>url</EncodingType><Contents><Key>other%2Fsecret</Key><Size>1</Size></Contents></ListBucketResult>`, strings.Repeat("x", (4<<20)+1)} {
		server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, body) }))
		c, err := New(Config{BucketURL: server.URL, Region: "auto", AccessKey: "key", SecretKey: "secret", AllowPrivate: true, AllowInsecureHTTP: true})
		if err != nil {
			t.Fatal(err)
		}
		_, err = c.ListPage(context.Background(), "docs/", "")
		if err == nil || strings.Contains(err.Error(), "X-Amz") {
			t.Fatal("invalid response accepted", err)
		}
		c.Close()
		server.Close()
	}
}
