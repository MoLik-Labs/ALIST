//go:build linux

package localfs

import (
	"errors"
	"os"
	"strings"
	"syscall"
	"unsafe"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

var ErrMoveUnsupported = errors.New("atomic non-overwriting move is unsupported")

// MoveNew atomically moves a file or directory to a new, nonexistent path.
// Paths are resolved relative to the anchored root and symlinks are not followed.
// Source inspection and rename are separate operations: the root must not be
// writable by untrusted local users who can replace directory entries during it.
// Cross-filesystem moves and filesystems lacking RENAME_NOREPLACE fail closed.
func (s *Store) MoveNew(source, destination string) error {
	from, err := components(source)
	if err != nil {
		return err
	}
	to, err := components(destination)
	if err != nil {
		return err
	}
	if len(from) == 0 || len(to) == 0 || source == destination || strings.HasPrefix(destination, source+"/") {
		return ErrInvalidPath
	}
	if !MoveSupported() {
		return ErrMoveUnsupported
	}
	fromParent, fromName := vpath.Split(source)
	toParent, toName := vpath.Split(destination)
	src, err := s.open(fromParent, true)
	if err != nil {
		return err
	}
	defer src.Close()
	dst, err := s.open(toParent, true)
	if err != nil {
		return err
	}
	defer dst.Close()
	fd, err := syscall.Openat(int(src.Fd()), fromName, oPath|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0)
	if err != nil {
		return err
	}
	entry := os.NewFile(uintptr(fd), fromName)
	if entry == nil {
		syscall.Close(fd)
		return errors.New("invalid descriptor")
	}
	info, err := entry.Stat()
	entry.Close()
	if err != nil {
		return err
	}
	if !info.IsDir() && !info.Mode().IsRegular() {
		return ErrNotRegular
	}
	oldp, err := syscall.BytePtrFromString(fromName)
	if err != nil {
		return err
	}
	newp, err := syscall.BytePtrFromString(toName)
	if err != nil {
		return err
	}
	const renameNoreplace = 1
	_, _, errno := syscall.Syscall6(renameat2Number, src.Fd(), uintptr(unsafe.Pointer(oldp)), dst.Fd(), uintptr(unsafe.Pointer(newp)), renameNoreplace, 0)
	switch errno {
	case 0:
		return nil
	case syscall.EEXIST:
		return ErrAlreadyExists
	case syscall.ENOSYS, syscall.EOPNOTSUPP, syscall.EINVAL:
		return ErrMoveUnsupported
	default:
		return errno
	}
}
