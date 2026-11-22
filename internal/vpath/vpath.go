// Package vpath implements Kova's virtual path semantics. It does not resolve
// filesystem paths or protect against symlinks; local drivers must do that
// separately at the point of access.
package vpath

import "strings"

// Clean normalizes both slash styles and rejects ASCII control characters.
// Parent components at the root are discarded, matching the native server.
// Input is already decoded: URL decoding must happen exactly once upstream.
func Clean(raw string) (string, bool) {
	for i := 0; i < len(raw); i++ {
		if raw[i] < 0x20 || raw[i] == 0x7f {
			return "", false
		}
	}
	parts := make([]string, 0, 8)
	for i := 0; i < len(raw); {
		for i < len(raw) && (raw[i] == '/' || raw[i] == '\\') {
			i++
		}
		start := i
		for i < len(raw) && raw[i] != '/' && raw[i] != '\\' {
			i++
		}
		if start == i {
			break
		}
		switch part := raw[start:i]; part {
		case ".":
		case "..":
			if len(parts) != 0 {
				parts = parts[:len(parts)-1]
			}
		default:
			parts = append(parts, part)
		}
	}
	if len(parts) == 0 {
		return "/", true
	}
	return "/" + strings.Join(parts, "/"), true
}

// Join normalizes base and rel together, as the native implementation does.
func Join(base, rel string) (string, bool) { return Clean(base + "/" + rel) }

// Split returns the parent and filename of an already-cleaned virtual path.
func Split(clean string) (string, string) {
	if clean == "" || clean == "/" {
		return "/", ""
	}
	i := strings.LastIndexByte(clean, '/')
	if i < 0 {
		return "/", clean
	}
	if i == 0 {
		return "/", clean[1:]
	}
	return clean[:i], clean[i+1:]
}

// IsSubpath checks a component boundary, not merely a string prefix.
// Both inputs must already be cleaned.
func IsSubpath(scope, target string) bool {
	if scope == "" || target == "" {
		return false
	}
	if scope == "/" {
		return target[0] == '/'
	}
	return target == scope || (len(target) > len(scope) && strings.HasPrefix(target, scope) && target[len(scope)] == '/')
}

// ValidFilename applies Kova's byte-length and separator restrictions.
func ValidFilename(name string) bool {
	if len(name) == 0 || len(name) > 255 || name == "." || name == ".." {
		return false
	}
	for i := 0; i < len(name); i++ {
		c := name[i]
		if c < 0x20 || c == 0x7f || c == '/' || c == '\\' {
			return false
		}
	}
	return true
}
