// Package cache provides the bounded, sharded LRU/TTL cache used by Kova.
package cache

import (
	"container/list"
	"hash/maphash"
	"strings"
	"sync"
	"time"
)

const shardCount = 64

type entry[V any] struct {
	key     string
	value   V
	expires time.Time
}

type shard[V any] struct {
	mu           sync.Mutex
	items        map[string]*list.Element
	lru          list.List
	hits, misses uint64
}

// Cache stores values by value; callers must synchronize mutable values they
// put into it. Capacity is per shard, matching the native implementation.
type Cache[V any] struct {
	seed     maphash.Seed
	capacity int
	shards   [shardCount]shard[V]
}

func New[V any](capacityPerShard int) *Cache[V] {
	if capacityPerShard < 4 {
		capacityPerShard = 4
	}
	return &Cache[V]{seed: maphash.MakeSeed(), capacity: capacityPerShard}
}

func (c *Cache[V]) shard(key string) *shard[V] {
	return &c.shards[maphash.String(c.seed, key)%shardCount]
}

func (c *Cache[V]) Get(key string) (V, bool) {
	s := c.shard(key)
	s.mu.Lock()
	defer s.mu.Unlock()
	item, ok := s.items[key]
	if !ok {
		s.misses++
		var zero V
		return zero, false
	}
	e := item.Value.(entry[V])
	if !time.Now().Before(e.expires) {
		delete(s.items, key)
		s.lru.Remove(item)
		s.misses++
		var zero V
		return zero, false
	}
	s.lru.MoveToFront(item)
	s.hits++
	return e.value, true
}

func (c *Cache[V]) Put(key string, value V, ttl time.Duration) {
	if ttl <= 0 {
		return
	}
	expires := time.Now().Add(ttl)
	s := c.shard(key)
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.items == nil {
		s.items = make(map[string]*list.Element)
	}
	if item, ok := s.items[key]; ok {
		item.Value = entry[V]{key, value, expires}
		s.lru.MoveToFront(item)
		return
	}
	if s.lru.Len() >= c.capacity {
		last := s.lru.Back()
		delete(s.items, last.Value.(entry[V]).key)
		s.lru.Remove(last)
	}
	s.items[key] = s.lru.PushFront(entry[V]{key, value, expires})
}

func (c *Cache[V]) Erase(key string) {
	s := c.shard(key)
	s.mu.Lock()
	defer s.mu.Unlock()
	if item, ok := s.items[key]; ok {
		delete(s.items, key)
		s.lru.Remove(item)
	}
}

// ErasePrefix invalidates a path and its descendants, including compound
// cache keys whose path is followed by NUL, but not similarly prefixed names.
// Operations across different shards are not atomic as a group.
func (c *Cache[V]) ErasePrefix(prefix string) {
	for i := range c.shards {
		s := &c.shards[i]
		s.mu.Lock()
		for item := s.lru.Front(); item != nil; {
			next := item.Next()
			key := item.Value.(entry[V]).key
			if key == prefix || (len(key) > len(prefix) && strings.HasPrefix(key, prefix) && (prefix == "/" || key[len(prefix)] == '/' || key[len(prefix)] == 0)) {
				delete(s.items, key)
				s.lru.Remove(item)
			}
			item = next
		}
		s.mu.Unlock()
	}
}

func (c *Cache[V]) Clear() {
	for i := range c.shards {
		s := &c.shards[i]
		s.mu.Lock()
		clear(s.items)
		s.lru.Init()
		s.mu.Unlock()
	}
}

// Stats returns a per-shard snapshot, not a globally atomic snapshot.
func (c *Cache[V]) Stats() (size int, hits, misses uint64) {
	for i := range c.shards {
		s := &c.shards[i]
		s.mu.Lock()
		size += s.lru.Len()
		hits += s.hits
		misses += s.misses
		s.mu.Unlock()
	}
	return
}
