//go:build linux

package localfs

import (
	"errors"
	"os"
	"syscall"
	"unsafe"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

var ErrDirectoryNotEmpty = errors.New("directory is not empty")

// Delete removes a regular file or an empty directory, never the root itself.
// It never follows a symlink and never recursively removes directory contents.
// The storage root must not be writable by untrusted local OS users: an actor
// who can rename its contents can change which entry is removed during a race.
func (s *Store) Delete(target string) error {
	parts, err := components(target)
	if err != nil {
		return err
	}
	if len(parts) == 0 {
		return ErrInvalidPath
	}
	parent, name := vpath.Split(target)
	dir, err := s.open(parent, true)
	if err != nil {
		return err
	}
	defer dir.Close()
	fd, err := syscall.Openat(int(dir.Fd()), name, oPath|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0)
	if err != nil {
		return err
	}
	file := os.NewFile(uintptr(fd), name)
	if file == nil {
		syscall.Close(fd)
		return errors.New("invalid descriptor")
	}
	info, err := file.Stat()
	file.Close()
	if err != nil {
		return err
	}
	if !info.IsDir() && !info.Mode().IsRegular() {
		return ErrNotRegular
	}
	if !info.IsDir() {
		return syscall.Unlinkat(int(dir.Fd()), name)
	}
	p, err := syscall.BytePtrFromString(name)
	if err != nil {
		return err
	}
	const atRemovedir = 0x200
	_, _, errno := syscall.Syscall(syscall.SYS_UNLINKAT, dir.Fd(), uintptr(unsafe.Pointer(p)), atRemovedir)
	if errno == syscall.ENOTEMPTY || errno == syscall.EEXIST {
		return ErrDirectoryNotEmpty
	}
	if errno != 0 {
		return errno
	}
	return nil
}
