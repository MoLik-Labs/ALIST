package outbound

import (
	"context"
	"errors"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/netip"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

type fakeResolver struct {
	addresses []netip.Addr
	calls     atomic.Int32
}

func (r *fakeResolver) LookupNetIP(_ context.Context, _, _ string) ([]netip.Addr, error) {
	r.calls.Add(1)
	return r.addresses, nil
}

func TestPublicIP(t *testing.T) {
	for _, tc := range []struct {
		ip   string
		want bool
	}{
		{"8.8.8.8", true}, {"2606:4700:4700::1111", true}, {"::ffff:8.8.8.8", true},
		{"127.0.0.1", false}, {"10.1.2.3", false}, {"169.254.169.254", false},
		{"100.64.0.1", false}, {"192.0.2.1", false}, {"198.51.100.2", false}, {"203.0.113.1", false},
		{"::1", false}, {"::ffff:127.0.0.1", false}, {"::127.0.0.1", false}, {"fc00::1", false},
		{"fe80::1%eth0", false}, {"2001:db8::1", false}, {"2002:c0a8:0101::", false}, {"64:ff9b::a00:1", false},
	} {
		ip := netip.MustParseAddr(tc.ip)
		if got := PublicIP(ip); got != tc.want {
			t.Errorf("PublicIP(%s) = %v", tc.ip, got)
		}
	}
	if PublicIP(netip.Addr{}) {
		t.Fatal("invalid IP accepted")
	}
}

func TestRejectMixedDNSAndBadURLs(t *testing.T) {
	resolver := &fakeResolver{addresses: []netip.Addr{netip.MustParseAddr("8.8.8.8"), netip.MustParseAddr("127.0.0.1")}}
	c := New(Options{Resolver: resolver, Timeout: time.Second})
	defer c.CloseIdleConnections()
	req, _ := http.NewRequest(http.MethodGet, "http://example.test/", nil)
	_, err := c.Do(req)
	if !errors.Is(err, ErrUnsafeAddress) || resolver.calls.Load() != 1 {
		t.Fatalf("mixed DNS not rejected: %v", err)
	}
	for _, raw := range []string{"ftp://example.test/a", "http://user:pass@example.test/", "http://127.0.0.1:0/"} {
		req, err := http.NewRequest(http.MethodGet, raw, nil)
		if err != nil {
			continue
		}
		if _, err := c.Do(req); !errors.Is(err, ErrInvalidURL) {
			t.Errorf("accepted %s: %v", raw, err)
		}
	}
}

func TestPinnedRedirectsAndCredentials(t *testing.T) {
	var leak atomic.Bool
	var server *httptest.Server
	server = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, port, _ := net.SplitHostPort(server.Listener.Addr().String())
		switch r.URL.Path {
		case "/start":
			if r.Header.Get("X-Api-Key") != "secret" {
				leak.Store(true)
			}
			http.Redirect(w, r, "http://b.test:"+port+"/hop", http.StatusFound)
		case "/hop":
			if r.Header.Get("X-Api-Key") != "" || r.Header.Get("Authorization") != "" || r.Header.Get("Accept") != "text/plain" {
				leak.Store(true)
			}
			http.Redirect(w, r, "http://a.test:"+port+"/end", http.StatusFound)
		case "/end":
			if r.Header.Get("X-Api-Key") != "" || r.Header.Get("Authorization") != "" {
				leak.Store(true)
			}
			_, _ = io.WriteString(w, "ok")
		case "/post":
			http.Redirect(w, r, "http://b.test:"+port+"/end", http.StatusTemporaryRedirect)
		case "/same":
			http.Redirect(w, r, "/same-end", http.StatusFound)
		case "/same-end":
			if r.Header.Get("X-Api-Key") != "secret" {
				leak.Store(true)
			}
			_, _ = io.WriteString(w, "ok")
		}
	}))
	defer server.Close()
	_, port, _ := net.SplitHostPort(server.Listener.Addr().String())
	resolver := &fakeResolver{addresses: []netip.Addr{netip.MustParseAddr("127.0.0.1")}}
	c := New(Options{AllowPrivate: true, FollowRedirects: true, Resolver: resolver, Timeout: 5 * time.Second})
	defer c.CloseIdleConnections()
	req, _ := http.NewRequest(http.MethodGet, "http://a.test:"+port+"/start", nil)
	req.Header.Set("X-Api-Key", "secret")
	req.Header.Set("Authorization", "Bearer secret")
	req.Header.Set("Accept", "text/plain")
	resp, err := c.Do(req)
	if err != nil {
		t.Fatal(err)
	}
	body, _ := io.ReadAll(resp.Body)
	resp.Body.Close()
	if resp.StatusCode != 200 || string(body) != "ok" || leak.Load() || resolver.calls.Load() < 2 {
		t.Fatalf("redirect status=%d body=%q leaked=%v resolutions=%d", resp.StatusCode, body, leak.Load(), resolver.calls.Load())
	}
	same, _ := http.NewRequest(http.MethodGet, "http://a.test:"+port+"/same", nil)
	same.Header.Set("X-Api-Key", "secret")
	resp, err = c.Do(same)
	if err != nil {
		t.Fatal(err)
	}
	resp.Body.Close()
	if resp.StatusCode != 200 || leak.Load() {
		t.Fatal("same-origin credentials lost")
	}
	post, _ := http.NewRequest(http.MethodPost, "http://a.test:"+port+"/post", strings.NewReader("payload"))
	resp, err = c.Do(post)
	if err != nil {
		t.Fatal(err)
	}
	resp.Body.Close()
	if resp.StatusCode != http.StatusTemporaryRedirect {
		t.Fatalf("unsafe request followed redirect: %d", resp.StatusCode)
	}
}

func TestPrivateOptInAndRedirectBlocked(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "http://127.0.0.1:1/private", http.StatusFound)
	}))
	defer server.Close()
	blocked := New(Options{Timeout: time.Second})
	defer blocked.CloseIdleConnections()
	req, _ := http.NewRequest(http.MethodGet, server.URL, nil)
	if _, err := blocked.Do(req); !errors.Is(err, ErrUnsafeAddress) {
		t.Fatalf("private target not rejected: %v", err)
	}
	// Simulate an already-validated public origin with a local test server; all
	// subsequent dials still use the guarded resolver/dialer.
	guarded := New(Options{FollowRedirects: true, Timeout: time.Second})
	defer guarded.CloseIdleConnections()
	originalDial := guarded.transport.DialContext
	guarded.transport.DialContext = func(ctx context.Context, network, address string) (net.Conn, error) {
		host, _, err := net.SplitHostPort(address)
		if err == nil && host == "public.test" {
			var dialer net.Dialer
			return dialer.DialContext(ctx, "tcp", server.Listener.Addr().String())
		}
		return originalDial(ctx, network, address)
	}
	initial, _ := http.NewRequest(http.MethodGet, "http://public.test/start", nil)
	if _, err := guarded.Do(initial); !errors.Is(err, ErrUnsafeAddress) {
		t.Fatalf("redirect to private target not rejected: %v", err)
	}
}
