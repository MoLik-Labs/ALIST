# PBKDF2

Unmodified `pbkdf2.go` and its test vectors from Go's `golang.org/x/crypto`
repository, tag `v0.31.0`:
https://github.com/golang/crypto/tree/v0.31.0/pbkdf2

Included locally to build without a network dependency and to support Go 1.22.
See `LICENSE` for the Go Authors' BSD license. This package is an internal
implementation detail; callers must validate iterations and input sizes.
