// Package mounts implements virtual mount routing. It does not authorize access
// or perform filesystem operations; callers must enforce those separately.
package mounts

import (
	"errors"
	"sort"
	"strings"
	"sync/atomic"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

var ErrInvalidMount = errors.New("mount paths must be unique, canonical virtual paths")

type Mount[V any] struct {
	Path  string
	Value V
}

type entry[V any] struct {
	mount    Mount[V]
	replicas []Mount[V]
}

type snapshot[V any] struct {
	entries  map[string]entry[V]
	children map[string][]string
}

// Table allows lock-free lookups against immutable snapshots. Values referenced
// by a mount must remain valid while in-flight readers use them. Concurrent
// Replace calls are safe but the last store wins, not necessarily the last call.
// A Table must not be copied after use.
type Table[V any] struct {
	current    atomic.Pointer[snapshot[V]]
	roundRobin atomic.Uint64
}

// Replace validates and builds a complete new table before publishing it.
// A failed replacement leaves the previous table untouched.
func (t *Table[V]) Replace(mounts []Mount[V]) error {
	s := &snapshot[V]{entries: make(map[string]entry[V], len(mounts)), children: make(map[string][]string)}
	ordered := make([]Mount[V], len(mounts))
	copy(ordered, mounts)
	sort.Slice(ordered, func(i, j int) bool { return ordered[i].Path < ordered[j].Path })
	for i, m := range ordered {
		clean, ok := vpath.Clean(m.Path)
		if !ok || clean != m.Path || (i > 0 && ordered[i-1].Path == m.Path) {
			return ErrInvalidMount
		}
		s.entries[m.Path] = entry[V]{mount: m}
	}
	for path, e := range s.entries {
		// Native Kova excludes any mount containing .balance as a primary.
		if strings.Contains(path, ".balance") {
			continue
		}
		prefix := path + ".balance"
		start := sort.Search(len(ordered), func(i int) bool { return ordered[i].Path >= prefix })
		for i := start; i < len(ordered) && strings.HasPrefix(ordered[i].Path, prefix); i++ {
			if len(ordered[i].Path) > len(prefix) {
				e.replicas = append(e.replicas, ordered[i])
			}
		}
		s.entries[path] = e
	}
	children := make(map[string]map[string]struct{})
	for _, m := range ordered {
		if m.Path == "/" || strings.Contains(m.Path, ".balance") {
			continue
		}
		parent := "/"
		for _, part := range strings.Split(strings.TrimPrefix(m.Path, "/"), "/") {
			if children[parent] == nil {
				children[parent] = make(map[string]struct{})
			}
			children[parent][part] = struct{}{}
			if parent == "/" {
				parent += part
			} else {
				parent += "/" + part
			}
		}
	}
	for parent, set := range children {
		list := make([]string, 0, len(set))
		for name := range set {
			list = append(list, name)
		}
		sort.Strings(list)
		s.children[parent] = list
	}
	t.current.Store(s)
	return nil
}

// Match returns the longest primary mount and a path relative to it. A replica
// is selected round-robin in lexical order after the primary, with one shared
// counter across all mounts as in native Kova. Input must already be canonical.
func (t *Table[V]) Match(cleanPath string) (mount Mount[V], subpath string, ok bool) {
	if !vpath.IsClean(cleanPath) {
		return mount, "", false
	}
	s := t.current.Load()
	if s == nil {
		return mount, "", false
	}
	probe := cleanPath
	for {
		if e, found := s.entries[probe]; found && !strings.Contains(probe, ".balance") {
			sub := "/"
			if probe == "/" {
				sub = cleanPath
			} else if probe != cleanPath {
				sub = cleanPath[len(probe):]
			}
			if len(e.replicas) == 0 {
				return e.mount, sub, true
			}
			selected := (t.roundRobin.Add(1) - 1) % uint64(len(e.replicas)+1)
			if selected == 0 {
				return e.mount, sub, true
			}
			return e.replicas[selected-1], sub, true
		}
		if probe == "/" {
			break
		}
		i := strings.LastIndexByte(probe, '/')
		if i == 0 {
			probe = "/"
		} else {
			probe = probe[:i]
		}
	}
	return mount, "", false
}

// Children returns sorted virtual child names, independently owned by the
// caller. Input must already be canonical; invalid paths return nil.
func (t *Table[V]) Children(cleanPath string) []string {
	if !vpath.IsClean(cleanPath) {
		return nil
	}
	s := t.current.Load()
	if s == nil {
		return nil
	}
	return append([]string(nil), s.children[cleanPath]...)
}
