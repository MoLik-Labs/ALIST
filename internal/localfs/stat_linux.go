//go:build linux

package localfs

import "os"

// Stat returns metadata for a regular file or directory without following the
// final symlink. It is not a substitute for opening the same inode when reading.
func (s *Store) Stat(path string) (Entry, error) {
	file, err := s.open(path, false)
	if err != nil {
		return Entry{}, err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return Entry{}, err
	}
	if !info.IsDir() && !info.Mode().IsRegular() {
		return Entry{}, os.ErrNotExist
	}
	return Entry{Name: info.Name(), Size: info.Size(), IsDir: info.IsDir(), Modified: info.ModTime().Unix()}, nil
}
