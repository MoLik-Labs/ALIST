//go:build linux

package localfs

import (
	"errors"
	"io"
	"os"
	"path/filepath"
	"sync"
	"testing"
)

func TestLocalStore(t *testing.T) {
	root := t.TempDir()
	outside := t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "folder"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "folder", "file.txt"), []byte("hello"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(outside, "secret"), []byte("secret"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(outside, filepath.Join(root, "escape")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(outside, "secret"), filepath.Join(root, "link")); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	entries, err := s.List("/")
	if err != nil || len(entries) != 1 || entries[0].Name != "folder" || !entries[0].IsDir {
		t.Fatal(entries, err)
	}
	f, info, err := s.OpenFile("/folder/file.txt")
	if err != nil {
		t.Fatal(err)
	}
	body, err := io.ReadAll(f)
	f.Close()
	if err != nil || string(body) != "hello" || info.Size() != 5 {
		t.Fatal(string(body), info, err)
	}
	for _, path := range []string{"/escape/secret", "/link", "/../secret", "/folder/../../secret", "/folder\\file.txt", "relative", "/folder/", "/folder/./file.txt"} {
		f, _, err := s.OpenFile(path)
		if err == nil {
			f.Close()
			t.Errorf("opened unsafe path %q", path)
		}
	}
	if f, _, err := s.OpenFile("/folder"); !errors.Is(err, ErrNotRegular) {
		if f != nil {
			f.Close()
		}
		t.Fatal("directory opened as file", err)
	}
	if err := s.Close(); err != nil {
		t.Fatal(err)
	}
	if _, err := s.List("/"); !errors.Is(err, ErrClosed) {
		t.Fatal("closed store accepted request", err)
	}
}

func TestSymlinkSwapDoesNotEscape(t *testing.T) {
	root, outside := t.TempDir(), t.TempDir()
	if err := os.Mkdir(filepath.Join(root, "dir"), 0700); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "dir", "file"), []byte("inside"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(outside, "file"), []byte("outside"), 0600); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for i := 0; i < 500; i++ {
			if os.Rename(filepath.Join(root, "dir"), filepath.Join(root, "held")) != nil {
				continue
			}
			_ = os.Symlink(outside, filepath.Join(root, "dir"))
			_ = os.Remove(filepath.Join(root, "dir"))
			_ = os.Rename(filepath.Join(root, "held"), filepath.Join(root, "dir"))
		}
	}()
	for i := 0; i < 1000; i++ {
		f, _, err := s.OpenFile("/dir/file")
		if err == nil {
			data, readErr := io.ReadAll(f)
			f.Close()
			if readErr != nil || string(data) != "inside" {
				t.Errorf("escaped root: %q %v", data, readErr)
				break
			}
		}
	}
	wg.Wait()
}

func TestConcurrentReadsAndClose(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "a"), []byte("x"), 0600); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 100; j++ {
				f, _, err := s.OpenFile("/a")
				if err == nil {
					f.Close()
				} else if !errors.Is(err, ErrClosed) {
					t.Error(err)
					return
				}
			}
		}()
	}
	if err := s.Close(); err != nil {
		t.Fatal(err)
	}
	wg.Wait()
}

func TestListLimitBoundsRawEntries(t *testing.T) {
	root := t.TempDir()
	for _, name := range []string{"a", "b", "c"} {
		if err := os.WriteFile(filepath.Join(root, name), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	if _, err := s.ListLimit("/", 2); !errors.Is(err, ErrTooManyEntries) {
		t.Fatalf("expected limit, got %v", err)
	}
	if entries, err := s.ListLimit("/", 3); err != nil || len(entries) != 3 {
		t.Fatalf("exact limit: %v %v", entries, err)
	}
	if _, err := s.ListLimit("/", 0); err == nil {
		t.Fatal("zero limit accepted")
	}
}
