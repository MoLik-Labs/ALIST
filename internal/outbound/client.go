// Package outbound implements a guarded HTTP(S) client for untrusted URLs.
// It is not yet wired into a Kova server or its storage drivers.
package outbound

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"net/netip"
	"net/url"
	"strconv"
	"strings"
	"time"
)

var ErrUnsafeAddress = errors.New("outbound address is private or reserved")
var ErrInvalidURL = errors.New("invalid outbound URL")

// Resolver returns all addresses for a hostname. A resolver must not hide
// private results: the entire answer is rejected if any address is unsafe.
type Resolver interface {
	LookupNetIP(ctx context.Context, network, host string) ([]netip.Addr, error)
}

type Options struct {
	AllowPrivate    bool
	FollowRedirects bool
	Timeout         time.Duration
	Resolver        Resolver // nil uses the system resolver
}

type Client struct {
	client    *http.Client
	transport *http.Transport
}

var blockedPrefixes = mustPrefixes(
	"0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16",
	"172.16.0.0/12", "192.0.0.0/24", "192.0.2.0/24", "192.88.99.0/24", "192.168.0.0/16",
	"198.18.0.0/15", "198.51.100.0/24", "203.0.113.0/24", "224.0.0.0/4", "240.0.0.0/4",
	"2001::/23", "2001:db8::/32", "2002::/16", "3fff::/20",
)
var globalV6 = netip.MustParsePrefix("2000::/3")

func mustPrefixes(ss ...string) []netip.Prefix {
	out := make([]netip.Prefix, 0, len(ss))
	for _, s := range ss {
		out = append(out, netip.MustParsePrefix(s))
	}
	return out
}

// PublicIP conservatively classifies IPs for outbound destinations. IPv4-mapped
// addresses are checked as IPv4; transition and documentation ranges are denied.
func PublicIP(ip netip.Addr) bool {
	if !ip.IsValid() || ip.Zone() != "" {
		return false
	}
	ip = ip.Unmap()
	if !ip.IsGlobalUnicast() {
		return false
	}
	if ip.Is6() && !globalV6.Contains(ip) {
		return false
	}
	for _, prefix := range blockedPrefixes {
		if prefix.Contains(ip) {
			return false
		}
	}
	return true
}

func validateURL(u *url.URL) error {
	if u == nil || (u.Scheme != "http" && u.Scheme != "https") || u.Opaque != "" || u.Host == "" || u.User != nil || u.Hostname() == "" {
		return ErrInvalidURL
	}
	for _, c := range u.String() {
		if c <= 0x20 || c == 0x7f {
			return ErrInvalidURL
		}
	}
	if strings.Contains(u.Hostname(), "%") {
		return ErrInvalidURL
	}
	if port := u.Port(); port != "" {
		p, err := strconv.Atoi(port)
		if err != nil || p < 1 || p > 65535 {
			return ErrInvalidURL
		}
	} else if strings.HasSuffix(u.Host, ":") {
		return ErrInvalidURL
	}
	return nil
}

func origin(u *url.URL) string {
	port := u.Port()
	if port == "" {
		if u.Scheme == "https" {
			port = "443"
		} else {
			port = "80"
		}
	}
	return strings.ToLower(u.Scheme) + "://" + net.JoinHostPort(strings.ToLower(u.Hostname()), port)
}

// New creates a reusable client. It disables proxy environment variables so
// DNS validation cannot be bypassed by an implicit proxy. DNS is resolved and
// checked immediately before each new connection, which is dialed by IP; TLS
// still authenticates the original URL hostname. Call CloseIdleConnections
// when the client is no longer needed.
func New(opts Options) *Client {
	resolver := opts.Resolver
	if resolver == nil {
		resolver = net.DefaultResolver
	}
	dialer := &net.Dialer{Timeout: 15 * time.Second, KeepAlive: 30 * time.Second}
	tr := &http.Transport{
		Proxy:                 nil,
		MaxIdleConns:          100,
		IdleConnTimeout:       90 * time.Second,
		TLSHandshakeTimeout:   10 * time.Second,
		ExpectContinueTimeout: time.Second,
		ForceAttemptHTTP2:     true,
	}
	tr.DialContext = func(ctx context.Context, network, address string) (net.Conn, error) {
		host, port, err := net.SplitHostPort(address)
		if err != nil {
			return nil, ErrInvalidURL
		}
		var addresses []netip.Addr
		if ip, err := netip.ParseAddr(host); err == nil {
			addresses = []netip.Addr{ip}
		} else {
			addresses, err = resolver.LookupNetIP(ctx, "ip", host)
			if err != nil {
				return nil, fmt.Errorf("resolve outbound host: %w", err)
			}
		}
		if len(addresses) == 0 {
			return nil, errors.New("no outbound addresses")
		}
		for _, ip := range addresses {
			if !ip.IsValid() || ip.Zone() != "" || (!opts.AllowPrivate && !PublicIP(ip)) {
				return nil, ErrUnsafeAddress
			}
		}
		var last error
		for _, ip := range addresses {
			conn, err := dialer.DialContext(ctx, "tcp", net.JoinHostPort(ip.String(), port))
			if err == nil {
				return conn, nil
			}
			last = err
			if ctx.Err() != nil {
				break
			}
		}
		return nil, last
	}
	client := &http.Client{Transport: tr, Timeout: opts.Timeout}
	client.CheckRedirect = func(req *http.Request, via []*http.Request) error {
		if !opts.FollowRedirects || len(via) == 0 || (via[0].Method != http.MethodGet && via[0].Method != http.MethodHead) || via[0].Body != nil && via[0].Body != http.NoBody {
			return http.ErrUseLastResponse
		}
		if len(via) >= 10 {
			return errors.New("too many redirects")
		}
		if err := validateURL(req.URL); err != nil {
			return err
		}
		original := origin(via[0].URL)
		crossed := origin(req.URL) != original
		for _, previous := range via[1:] {
			if origin(previous.URL) != original {
				crossed = true
				break
			}
		}
		if crossed {
			for name := range req.Header {
				switch strings.ToLower(name) {
				case "accept", "accept-encoding", "accept-language", "user-agent", "range":
				default:
					req.Header.Del(name)
				}
			}
			req.Host = ""
		}
		return nil
	}
	return &Client{client: client, transport: tr}
}

// Do sends req; the caller must close the response body. Requests should carry
// a context or client timeout to bound stalled operations and streams.
func (c *Client) Do(req *http.Request) (*http.Response, error) {
	if req == nil {
		return nil, ErrInvalidURL
	}
	if err := validateURL(req.URL); err != nil {
		return nil, err
	}
	return c.client.Do(req)
}

func (c *Client) CloseIdleConnections() { c.transport.CloseIdleConnections() }
