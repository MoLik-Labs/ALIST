//go:build linux

package localfs

import (
	"context"
	"errors"
	"io"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
)

type failingReader struct{}

func (failingReader) Read([]byte) (int, error) { return 0, io.ErrUnexpectedEOF }

func TestPutNew(t *testing.T) {
	root := t.TempDir()
	outside := t.TempDir()
	if err := os.Symlink(outside, filepath.Join(root, "link")); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(filepath.Join(outside, "not-created"), filepath.Join(root, "target-link")); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	if n, err := s.PutNew(context.Background(), "/file", strings.NewReader("hello"), 5); err != nil || n != 5 {
		t.Fatal(n, err)
	}
	if _, err := s.PutNew(context.Background(), "/file", strings.NewReader("overwrite"), 100); !errors.Is(err, ErrAlreadyExists) {
		t.Fatal("overwrote existing", err)
	}
	if data, err := os.ReadFile(filepath.Join(root, "file")); err != nil || string(data) != "hello" {
		t.Fatal(string(data), err)
	}
	if _, err := s.PutNew(context.Background(), "/target-link", strings.NewReader("bad"), 10); !errors.Is(err, ErrAlreadyExists) {
		t.Fatal("replaced symlink", err)
	}
	if _, err := os.Stat(filepath.Join(outside, "not-created")); !errors.Is(err, os.ErrNotExist) {
		t.Fatal("followed destination symlink", err)
	}
	for _, path := range []string{"/../outside", "/link/file", "/" + temporaryPrefix + "fake"} {
		if _, err := s.PutNew(context.Background(), path, strings.NewReader("bad"), 10); err == nil {
			t.Fatalf("accepted unsafe path %q", path)
		}
	}
	if _, err := s.PutNew(context.Background(), "/large", strings.NewReader("123456"), 5); !errors.Is(err, ErrTooLarge) {
		t.Fatal("size", err)
	}
	if _, err := s.PutNew(context.Background(), "/failed", failingReader{}, 100); !errors.Is(err, io.ErrUnexpectedEOF) {
		t.Fatal("read error", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := s.PutNew(ctx, "/cancelled", strings.NewReader("data"), 100); !errors.Is(err, context.Canceled) {
		t.Fatal("cancel", err)
	}
	entries, err := os.ReadDir(root)
	if err != nil {
		t.Fatal(err)
	}
	for _, entry := range entries {
		if strings.HasPrefix(entry.Name(), temporaryPrefix) {
			t.Fatalf("temporary file leaked %q", entry.Name())
		}
	}
}

func TestPutNewConcurrent(t *testing.T) {
	root := t.TempDir()
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	var successes atomic.Int32
	var wg sync.WaitGroup
	for i := 0; i < 20; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := s.PutNew(context.Background(), "/file", strings.NewReader("test"), 4)
			if err == nil {
				successes.Add(1)
			} else if !errors.Is(err, ErrAlreadyExists) {
				t.Error(err)
			}
		}()
	}
	wg.Wait()
	if successes.Load() != 1 {
		t.Fatalf("successes %d", successes.Load())
	}
	entries, err := os.ReadDir(root)
	if err != nil || len(entries) != 1 {
		t.Fatal(entries, err)
	}
}

func TestTemporaryNamesHidden(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, temporaryPrefix+"orphan"), []byte("secret"), 0600); err != nil {
		t.Fatal(err)
	}
	s, err := Open(root)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	entries, err := s.List("/")
	if err != nil || len(entries) != 0 {
		t.Fatal(entries, err)
	}
	if f, _, err := s.OpenFile("/" + temporaryPrefix + "orphan"); err == nil {
		f.Close()
		t.Fatal("temporary file exposed")
	}
}
