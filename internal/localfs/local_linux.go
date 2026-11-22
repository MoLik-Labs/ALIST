//go:build linux

// Package localfs provides experimental Linux local storage using
// directory-relative descriptors; it never follows symlinks.
package localfs

import (
	"errors"
	"fmt"
	"io"
	"os"
	"sort"
	"strings"
	"sync"
	"syscall"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

var ErrInvalidPath = errors.New("invalid local path")
var ErrNotRegular = errors.New("not a regular file")
var ErrTooManyEntries = errors.New("directory has too many entries")
var ErrClosed = errors.New("storage is closed")

const maxEntries = 10000

// O_PATH is Linux-specific and lets us inspect an inode without opening devices.
const oPath = 0x200000

type Store struct {
	mu     sync.RWMutex
	root   int
	closed bool
}
type Entry struct {
	Name     string `json:"name"`
	Size     int64  `json:"size"`
	IsDir    bool   `json:"is_dir"`
	Modified int64  `json:"modified"`
}

// Open anchors an existing directory. It rejects a symlink as the root itself.
// The caller must close the Store after all requests have completed.
func Open(root string) (*Store, error) {
	fd, err := syscall.Open(root, syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0)
	if err != nil {
		return nil, err
	}
	return &Store{root: fd}, nil
}

func (s *Store) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed {
		return nil
	}
	s.closed = true
	return syscall.Close(s.root)
}

// components rejects traversal rather than normalizing it. Inputs are already
// URL-decoded; a literal percent sequence is treated as a filename, not decoded.
func components(path string) ([]string, error) {
	if !vpath.IsClean(path) {
		return nil, ErrInvalidPath
	}
	if path == "/" {
		return nil, nil
	}
	parts := strings.Split(path[1:], "/")
	for _, part := range parts {
		if !vpath.ValidFilename(part) || strings.HasPrefix(part, temporaryPrefix) {
			return nil, ErrInvalidPath
		}
	}
	return parts, nil
}

// open walks from the trusted root descriptor, refusing symlinks at every step.
// Its return value is an independent descriptor owned by the caller.
func (s *Store) open(path string, directory bool) (*os.File, error) {
	parts, err := components(path)
	if err != nil {
		return nil, err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.closed {
		return nil, ErrClosed
	}
	fd, err := syscall.Openat(s.root, ".", syscall.O_RDONLY|syscall.O_DIRECTORY|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0)
	if err != nil {
		return nil, err
	}
	for i, part := range parts {
		flags := syscall.O_RDONLY | syscall.O_CLOEXEC | syscall.O_NOFOLLOW | syscall.O_NONBLOCK
		if i < len(parts)-1 || directory {
			flags |= syscall.O_DIRECTORY
		} else {
			flags = oPath | syscall.O_CLOEXEC | syscall.O_NOFOLLOW
		}
		next, err := syscall.Openat(fd, part, flags, 0)
		syscall.Close(fd)
		if err != nil {
			return nil, err
		}
		fd = next
	}
	file := os.NewFile(uintptr(fd), path)
	if file == nil {
		syscall.Close(fd)
		return nil, errors.New("invalid descriptor")
	}
	return file, nil
}

// OpenFile returns a seekable regular file descriptor; the caller closes it.
// Descriptor-relative traversal keeps it inside the anchored root even if a
// directory is concurrently renamed or replaced with a symlink.
func (s *Store) OpenFile(path string) (*os.File, os.FileInfo, error) {
	f, err := s.open(path, false)
	if err != nil {
		return nil, nil, err
	}
	info, err := f.Stat()
	if err != nil {
		f.Close()
		return nil, nil, err
	}
	if !info.Mode().IsRegular() {
		f.Close()
		return nil, nil, ErrNotRegular
	}
	// Reopen the inspected inode, not its mutable original pathname. This needs
	// Linux procfs; without it, fail closed. The O_PATH descriptor pins the inode.
	fd, err := syscall.Open(fmt.Sprintf("/proc/self/fd/%d", f.Fd()), syscall.O_RDONLY|syscall.O_CLOEXEC|syscall.O_NONBLOCK, 0)
	f.Close()
	if err != nil {
		return nil, nil, err
	}
	opened := os.NewFile(uintptr(fd), path)
	if opened == nil {
		syscall.Close(fd)
		return nil, nil, errors.New("invalid descriptor")
	}
	info, err = opened.Stat()
	if err != nil {
		opened.Close()
		return nil, nil, err
	}
	return opened, info, nil
}

// List returns sorted metadata for non-symlink regular files and directories.
// Unsafe or disappearing entries are skipped; the raw directory size is capped.
func (s *Store) List(path string) ([]Entry, error) {
	return s.ListLimit(path, maxEntries)
}

// ListLimit bounds raw directory entries inspected, including hidden or unsafe
// entries. No partial listing is returned if the bound is exceeded.
func (s *Store) ListLimit(path string, limit int) ([]Entry, error) {
	if limit < 1 || limit > maxEntries {
		return nil, errors.New("invalid listing limit")
	}
	dir, err := s.open(path, true)
	if err != nil {
		return nil, err
	}
	defer dir.Close()
	var result []Entry
	seen := 0
	for {
		items, err := dir.ReadDir(256)
		seen += len(items)
		if seen > limit {
			return nil, ErrTooManyEntries
		}
		for _, item := range items {
			name := item.Name()
			if !vpath.ValidFilename(name) || strings.HasPrefix(name, temporaryPrefix) || item.Type()&os.ModeSymlink != 0 {
				continue
			}
			fd, e := syscall.Openat(int(dir.Fd()), name, oPath|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0)
			if e != nil {
				continue
			}
			f := os.NewFile(uintptr(fd), name)
			if f == nil {
				syscall.Close(fd)
				continue
			}
			info, e := f.Stat()
			f.Close()
			if e != nil || !(info.IsDir() || info.Mode().IsRegular()) {
				continue
			}
			result = append(result, Entry{Name: name, Size: info.Size(), IsDir: info.IsDir(), Modified: info.ModTime().Unix()})
		}
		if err != nil {
			if !errors.Is(err, io.EOF) {
				return nil, fmt.Errorf("read directory: %w", err)
			}
			break
		}
	}
	sort.Slice(result, func(i, j int) bool {
		if result[i].IsDir != result[j].IsDir {
			return result[i].IsDir
		}
		return result[i].Name < result[j].Name
	})
	if result == nil {
		result = []Entry{}
	}
	return result, nil
}
