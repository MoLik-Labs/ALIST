package main

import (
	"bytes"
	"net"
	"strconv"
	"strings"
	"testing"
)

func TestCLI(t *testing.T) {
	t.Setenv("ALIST_HTTP_PORT", "")
	t.Setenv("PORT", "")
	t.Setenv("KOVA_GO_ROOT", "")
	for _, name := range []string{"KOVA_GO_ALLOW_WRITE", "KOVA_GO_ALLOW_DELETE", "KOVA_GO_ALLOW_MOVE", "KOVA_GO_ENABLE_WEBDAV", "KOVA_GO_ENABLE_SHARES"} {
		t.Setenv(name, "")
	}
	for _, tc := range []struct {
		args []string
		want int
		text string
	}{
		{[]string{"version"}, 0, "incomplete"}, {[]string{"--help"}, 0, "experimental"},
		{nil, 2, "Usage"}, {[]string{"unknown"}, 2, "Usage"},
		{[]string{"server"}, 2, "requires --experimental"},
		{[]string{"server", "--experimental", "--port", "0"}, 2, "valid port"},
		{[]string{"server", "--experimental", "--allow-write"}, 2, "requires --root"},
		{[]string{"server", "--experimental", "--allow-delete"}, 2, "requires --root"},
		{[]string{"server", "--experimental", "--allow-move"}, 2, "requires --root"},
		{[]string{"server", "--experimental", "--enable-webdav"}, 2, "requires --root"},
		{[]string{"server", "--experimental", "--enable-shares"}, 2, "requires --root"},
	} {
		var out, stderr bytes.Buffer
		got := run(tc.args, &out, &stderr)
		if got != tc.want || !strings.Contains(out.String()+stderr.String(), tc.text) {
			t.Errorf("run(%q) = %d, stdout=%q stderr=%q", tc.args, got, &out, &stderr)
		}
	}
}

func TestPortPrecedence(t *testing.T) {
	t.Setenv("PORT", "8000")
	t.Setenv("ALIST_HTTP_PORT", "9000")
	if port, err := portFromEnvironment(); err != nil || port != 9000 {
		t.Fatal(port, err)
	}
	t.Setenv("ALIST_HTTP_PORT", "invalid")
	if _, err := portFromEnvironment(); err == nil {
		t.Fatal("invalid port accepted")
	}
}

func TestOperationEnvBooleans(t *testing.T) {
	for _, name := range []string{"KOVA_GO_ALLOW_WRITE", "KOVA_GO_ALLOW_DELETE", "KOVA_GO_ALLOW_MOVE", "KOVA_GO_ENABLE_WEBDAV", "KOVA_GO_ENABLE_SHARES"} {
		t.Run(name, func(t *testing.T) {
			for _, tc := range []struct {
				value   string
				want    bool
				invalid bool
			}{{"", false, false}, {"false", false, false}, {"true", true, false}, {"1", false, true}, {"TRUE", false, true}, {"yes", false, true}, {" false ", false, true}} {
				t.Setenv(name, tc.value)
				got, err := boolFromEnvironment(name)
				if got != tc.want || (err != nil) != tc.invalid {
					t.Errorf("%s=%q: %v %v", name, tc.value, got, err)
				}
			}
		})
	}
}

func TestOperationEnvRequiresRootAndFlagOverrides(t *testing.T) {
	t.Setenv("PORT", "")
	t.Setenv("ALIST_HTTP_PORT", "")
	t.Setenv("KOVA_GO_ROOT", "")
	t.Setenv("KOVA_GO_ALLOW_WRITE", "true")
	t.Setenv("KOVA_GO_ALLOW_DELETE", "false")
	t.Setenv("KOVA_GO_ALLOW_MOVE", "false")
	t.Setenv("KOVA_GO_ENABLE_WEBDAV", "false")
	t.Setenv("KOVA_GO_ENABLE_SHARES", "false")
	var out, errOut bytes.Buffer
	if got := run([]string{"server", "--experimental"}, &out, &errOut); got != 2 || !strings.Contains(errOut.String(), "requires --root") {
		t.Fatal(got, errOut.String())
	}
	t.Setenv("KOVA_GO_ALLOW_WRITE", "not-a-boolean")
	errOut.Reset()
	if got := run([]string{"server", "--experimental"}, &out, &errOut); got != 2 || !strings.Contains(errOut.String(), "KOVA_GO_ALLOW_WRITE") {
		t.Fatal(got, errOut.String())
	}
	t.Setenv("KOVA_GO_ALLOW_WRITE", "true")
	errOut.Reset()
	// Hold the port so a successful flag override reaches listen and fails without hanging.
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer listener.Close()
	port := strconv.Itoa(listener.Addr().(*net.TCPAddr).Port)
	if got := run([]string{"server", "--experimental", "--allow-write=false", "--port", port}, &out, &errOut); got != 1 || !strings.Contains(errOut.String(), "listen:") {
		t.Fatal(got, errOut.String())
	}
}
