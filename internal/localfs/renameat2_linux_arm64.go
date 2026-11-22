//go:build linux && arm64

package localfs

const renameat2Number = 276

func MoveSupported() bool { return true }
