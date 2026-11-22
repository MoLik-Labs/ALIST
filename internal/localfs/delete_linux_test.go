//go:build linux

package localfs

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
)

func TestDeleteNonrecursive(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "folder"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "folder", "file"), []byte("data"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(outside, "secret"), []byte("safe"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "link")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(outside, "secret"), filepath.Join(root, "filelink")); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	if err := s.Delete("/folder"); !errors.Is(err, ErrDirectoryNotEmpty) {
		t.Fatal("nonempty directory", err)
	}
	for _, p := range []string{"/", "/../secret", "/link/secret", "/filelink", "/.kova-upload-temp"} {
		if err := s.Delete(p); err == nil {
			t.Errorf("unsafe deletion %q", p)
		}
	}
	if err := s.Delete("/folder/file"); err != nil {
		t.Fatal(err)
	}
	if err := s.Delete("/folder"); err != nil {
		t.Fatal(err)
	}
	if data, err := os.ReadFile(filepath.Join(outside, "secret")); err != nil || string(data) != "safe" {
		t.Fatal("outside changed", err)
	}
}
