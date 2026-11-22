//go:build linux && !amd64 && !arm64

package localfs

const renameat2Number = 0

// MoveSupported reports whether the syscall is wired for this architecture.
// The filesystem and kernel may still reject the operation.
func MoveSupported() bool { return false }
