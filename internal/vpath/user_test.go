package vpath

import "testing"

func TestResolveUserPath(t *testing.T) {
	for _, tc := range []struct {
		base, request, want string
		ok                  bool
	}{
		{"/home/alice", "../../secret", "/home/alice/secret", true},
		{"/home/alice", "/docs/file", "/home/alice/docs/file", true},
		{"/home/alice", `\folder\..\x`, "/home/alice/x", true},
		{"/home/alice", "", "/home/alice", true},
		{"/home/alice", "\x00", "", false},
		{"bad\x00base", "/file", "/file", true},
	} {
		got, ok := ResolveUserPath(tc.base, tc.request)
		if got != tc.want || ok != tc.ok {
			t.Errorf("ResolveUserPath(%q, %q) = %q, %v; want %q, %v", tc.base, tc.request, got, ok, tc.want, tc.ok)
		}
	}
}

func FuzzResolveUserPath(f *testing.F) {
	f.Add("/home/alice", "../../x")
	f.Add("/", "abc")
	f.Fuzz(func(t *testing.T, base, request string) {
		result, ok := ResolveUserPath(base, request)
		if !ok {
			return
		}
		cleanBase, valid := Clean(base)
		if !valid {
			cleanBase = "/"
		}
		if !IsClean(result) || !IsSubpath(cleanBase, result) {
			t.Fatalf("escaped scope: %q under %q", result, cleanBase)
		}
	})
}
