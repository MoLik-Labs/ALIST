//go:build linux

package localfs

import (
	"errors"
	"syscall"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

// Mkdir creates a single directory, without following symlinks or creating
// missing parents. An existing target (including a symlink) is never replaced.
func (s *Store) Mkdir(target string) error {
	parts, err := components(target)
	if err != nil {
		return err
	}
	if len(parts) == 0 {
		return ErrAlreadyExists
	}
	parent, name := vpath.Split(target)
	dir, err := s.open(parent, true)
	if err != nil {
		return err
	}
	defer dir.Close()
	if err := syscall.Mkdirat(int(dir.Fd()), name, 0700); err != nil {
		if errors.Is(err, syscall.EEXIST) {
			return ErrAlreadyExists
		}
		return err
	}
	return nil
}
