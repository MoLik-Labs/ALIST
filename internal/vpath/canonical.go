package vpath

// IsClean reports whether s is already a canonical virtual path, without
// allocating. Its accepted byte strings are exactly the fixed points of Clean.
func IsClean(s string) bool {
	if len(s) == 0 || s[0] != '/' {
		return false
	}
	if s == "/" {
		return true
	}
	start := 1
	for i := 1; i <= len(s); i++ {
		if i < len(s) {
			c := s[i]
			if c < 0x20 || c == 0x7f || c == '\\' {
				return false
			}
			if c != '/' {
				continue
			}
		}
		part := s[start:i]
		if part == "" || part == "." || part == ".." {
			return false
		}
		start = i + 1
	}
	return true
}
