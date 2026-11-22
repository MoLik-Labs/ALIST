package vpath

// ResolveUserPath resolves a user-supplied path under a user's base path using
// the native server's rules. Both inputs are virtual, not filesystem, paths.
// The caller must separately enforce permissions and filesystem confinement.
func ResolveUserPath(base, requested string) (string, bool) {
	request, ok := Clean(requested)
	if !ok {
		return "", false
	}
	basePath, ok := Clean(base)
	if !ok {
		basePath = "/"
	} // Matches the native invalid-base fallback.
	joined, ok := Join(basePath, request)
	if !ok || !IsSubpath(basePath, joined) {
		return "", false
	}
	return joined, true
}
