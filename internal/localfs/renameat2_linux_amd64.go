//go:build linux && amd64

package localfs

// Linux x86-64 syscall number; see the Linux syscall table.
const renameat2Number = 316

func MoveSupported() bool { return true }
