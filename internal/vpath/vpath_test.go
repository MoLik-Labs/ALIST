package vpath

import (
	"strings"
	"testing"
)

func TestClean(t *testing.T) {
	for _, tc := range []struct {
		in, want string
		ok       bool
	}{
		{"", "/", true}, {"/", "/", true}, {"../a/../../b", "/b", true},
		{`a\b//./c/../d`, "/a/b/d", true}, {"/日本語/á", "/日本語/á", true},
		{"/%2e%2e/x", "/%2e%2e/x", true}, {"a\x00b", "", false},
		{"a\x1fb", "", false}, {"a\x7fb", "", false},
	} {
		got, ok := Clean(tc.in)
		if got != tc.want || ok != tc.ok {
			t.Errorf("Clean(%q) = %q, %v; want %q, %v", tc.in, got, ok, tc.want, tc.ok)
		}
	}
}

func TestJoinSplitAndScope(t *testing.T) {
	got, ok := Join("/a/b", "../c")
	if !ok || got != "/a/c" {
		t.Fatalf("Join = %q, %v", got, ok)
	}
	parent, name := Split(got)
	if parent != "/a" || name != "c" {
		t.Fatalf("Split = %q, %q", parent, name)
	}
	for _, tc := range []struct {
		scope, target string
		want          bool
	}{
		{"/a", "/a", true}, {"/a", "/a/b", true}, {"/a", "/abc", false},
		{"/", "/a", true}, {"/", "a", false}, {"", "/", false},
	} {
		if got := IsSubpath(tc.scope, tc.target); got != tc.want {
			t.Errorf("IsSubpath(%q,%q) = %v", tc.scope, tc.target, got)
		}
	}
}

func TestValidFilename(t *testing.T) {
	for _, s := range []string{"x", "日本語", strings.Repeat("a", 255)} {
		if !ValidFilename(s) {
			t.Errorf("rejected %q", s)
		}
	}
	for _, s := range []string{"", ".", "..", "a/b", `a\b`, "a\x00b", "a\x7fb", strings.Repeat("a", 256)} {
		if ValidFilename(s) {
			t.Errorf("accepted %q", s)
		}
	}
}

func FuzzClean(f *testing.F) {
	for _, s := range []string{"", "/a/../b", `a\b`, "a\x00b", "/é"} {
		f.Add(s)
	}
	f.Fuzz(func(t *testing.T, input string) {
		clean, ok := Clean(input)
		if IsClean(input) != (ok && clean == input) {
			t.Fatalf("IsClean(%q) disagrees with Clean: %q, %v", input, clean, ok)
		}
		if !ok {
			return
		}
		if !strings.HasPrefix(clean, "/") {
			t.Fatalf("not absolute: %q", clean)
		}
		again, ok := Clean(clean)
		if !ok || again != clean {
			t.Fatalf("not idempotent: %q -> %q", clean, again)
		}
	})
}
