//go:build linux && (amd64 || arm64)

package localfs

import (
	"errors"
	"os"
	"path/filepath"
	"sync"
	"sync/atomic"
	"testing"
)

func TestMoveNoReplace(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	for _, d := range []string{"a", "b"} {
		if err := os.Mkdir(filepath.Join(root, d), 0700); err != nil {
			t.Fatal(err)
		}
	}
	for n, text := range map[string]string{"a/file": "source", "b/existing": "keep", "a/other": "other"} {
		if err := os.WriteFile(filepath.Join(root, n), []byte(text), 0600); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.Symlink(outside, filepath.Join(root, "escape")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(outside, "secret"), filepath.Join(root, "a/link")); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	for _, tc := range []struct{ src, dst string }{{"/", "/x"}, {"/a/file", "/"}, {"/../outside", "/x"}, {"/a/link", "/b/link"}, {"/a/file", "/escape/x"}, {"/a/file", "/a/file"}, {"/a", "/a/subdir"}} {
		if err := s.MoveNew(tc.src, tc.dst); err == nil {
			t.Errorf("accepted unsafe move %q -> %q", tc.src, tc.dst)
		}
	}
	if err := s.MoveNew("/a/file", "/b/existing"); !errors.Is(err, ErrAlreadyExists) {
		t.Fatalf("overwrite: %v", err)
	}
	if b, _ := os.ReadFile(filepath.Join(root, "b/existing")); string(b) != "keep" {
		t.Fatal("overwritten")
	}
	if err := s.MoveNew("/a/file", "/b/file"); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(filepath.Join(root, "b/file")); string(b) != "source" {
		t.Fatal("wrong data")
	}
	if err := s.MoveNew("/a", "/b/moved-directory"); err != nil {
		t.Fatal(err)
	}
}

func TestMoveConcurrentNoReplace(t *testing.T) {
	root := t.TempDir()
	for _, n := range []string{"one", "two"} {
		if err := os.WriteFile(filepath.Join(root, n), []byte(n), 0600); err != nil {
			t.Fatal(err)
		}
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	var wg sync.WaitGroup
	var successes atomic.Int32
	for _, n := range []string{"one", "two"} {
		wg.Add(1)
		go func(n string) {
			defer wg.Done()
			err := s.MoveNew("/"+n, "/target")
			if err == nil {
				successes.Add(1)
			} else if !errors.Is(err, ErrAlreadyExists) {
				t.Error(err)
			}
		}(n)
	}
	wg.Wait()
	if successes.Load() != 1 {
		t.Fatalf("successes %d", successes.Load())
	}
}
