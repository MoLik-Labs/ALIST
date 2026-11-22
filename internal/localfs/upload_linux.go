//go:build linux

package localfs

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"io"
	"os"
	"strings"
	"syscall"
	"unsafe"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

const temporaryPrefix = ".kova-upload-"

var ErrTooLarge = errors.New("upload exceeds limit")
var ErrAlreadyExists = errors.New("destination already exists")

// PutNew streams a file to a private temporary file and atomically publishes
// it only if the destination does not exist. The parent must already exist.
// Filesystems that disallow hard links cause a failure; there is no unsafe
// fallback to an overwriting rename. A crash may leave a temporary file.
func (s *Store) PutNew(ctx context.Context, target string, src io.Reader, maxBytes int64) (int64, error) {
	parts, err := components(target)
	if err != nil || len(parts) == 0 || src == nil || maxBytes < 0 || maxBytes == int64(^uint64(0)>>1) {
		return 0, ErrInvalidPath
	}
	for _, part := range parts {
		if strings.HasPrefix(part, temporaryPrefix) {
			return 0, ErrInvalidPath
		}
	}
	parent, name := vpath.Split(target)
	dir, err := s.open(parent, true)
	if err != nil {
		return 0, err
	}
	defer dir.Close()
	var random [16]byte
	if _, err := rand.Read(random[:]); err != nil {
		return 0, err
	}
	temp := temporaryPrefix + hex.EncodeToString(random[:])
	fd, err := syscall.Openat(int(dir.Fd()), temp, syscall.O_CREAT|syscall.O_EXCL|syscall.O_WRONLY|syscall.O_CLOEXEC|syscall.O_NOFOLLOW, 0600)
	if err != nil {
		return 0, err
	}
	f := os.NewFile(uintptr(fd), temp)
	if f == nil {
		syscall.Close(fd)
		_ = syscall.Unlinkat(int(dir.Fd()), temp)
		return 0, errors.New("invalid upload descriptor")
	}
	defer func() { _ = f.Close(); _ = syscall.Unlinkat(int(dir.Fd()), temp) }()
	n, err := io.Copy(f, io.LimitReader(&contextReader{ctx: ctx, reader: src}, maxBytes+1))
	if err != nil {
		return n, err
	}
	if n > maxBytes {
		return n, ErrTooLarge
	}
	if err := ctx.Err(); err != nil {
		return n, err
	}
	if err := f.Sync(); err != nil {
		return n, err
	}
	if err := f.Close(); err != nil {
		return n, err
	}
	if err := ctx.Err(); err != nil {
		return n, err
	}
	if err := linkAt(int(dir.Fd()), temp, name); err != nil {
		if errors.Is(err, syscall.EEXIST) {
			return n, ErrAlreadyExists
		}
		return n, err
	}
	return n, nil
}

type contextReader struct {
	ctx    context.Context
	reader io.Reader
}

func (r *contextReader) Read(p []byte) (int, error) {
	if err := r.ctx.Err(); err != nil {
		return 0, err
	}
	n, err := r.reader.Read(p)
	if err == nil {
		if ctxErr := r.ctx.Err(); ctxErr != nil {
			return n, ctxErr
		}
	}
	return n, err
}

func linkAt(dirfd int, oldname, newname string) error {
	oldp, err := syscall.BytePtrFromString(oldname)
	if err != nil {
		return err
	}
	newp, err := syscall.BytePtrFromString(newname)
	if err != nil {
		return err
	}
	_, _, errno := syscall.Syscall6(syscall.SYS_LINKAT, uintptr(dirfd), uintptr(unsafe.Pointer(oldp)), uintptr(dirfd), uintptr(unsafe.Pointer(newp)), 0, 0)
	if errno != 0 {
		return errno
	}
	return nil
}
