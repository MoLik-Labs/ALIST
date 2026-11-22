package cache

import (
	"fmt"
	"sync"
	"testing"
	"time"
)

func TestSemantics(t *testing.T) {
	c := New[int](4)
	c.Put("/a", 1, time.Minute)
	c.Put("/a/b", 2, time.Minute)
	c.Put("/a\x00user", 3, time.Minute)
	c.Put("/abc", 4, time.Minute)
	c.Put("ignored", 0, 0)
	if v, ok := c.Get("/a"); !ok || v != 1 {
		t.Fatal(v, ok)
	}
	c.ErasePrefix("/a")
	for _, key := range []string{"/a", "/a/b", "/a\x00user", "ignored"} {
		if _, ok := c.Get(key); ok {
			t.Fatalf("unexpected key %q", key)
		}
	}
	if v, ok := c.Get("/abc"); !ok || v != 4 {
		t.Fatal(v, ok)
	}
	c.Clear()
	size, hits, misses := c.Stats()
	if size != 0 || hits != 2 || misses != 4 {
		t.Fatal(size, hits, misses)
	}
}

func TestExpirationAndEviction(t *testing.T) {
	c := New[int](4)
	// Find keys in one shard to exercise per-shard LRU deterministically.
	keys := []string{"0"}
	s := c.shard(keys[0])
	for i := 1; len(keys) < 5; i++ {
		k := fmt.Sprint(i)
		if c.shard(k) == s {
			keys = append(keys, k)
		}
	}
	for i := 0; i < 4; i++ {
		c.Put(keys[i], i, time.Minute)
	}
	c.Get(keys[0])
	c.Put(keys[4], 4, time.Minute)
	if _, ok := c.Get(keys[1]); ok {
		t.Fatal("least recently used key survived")
	}
	if _, ok := c.Get(keys[0]); !ok {
		t.Fatal("recent key evicted")
	}
	c.Put("expired", 1, time.Nanosecond)
	time.Sleep(time.Millisecond)
	if _, ok := c.Get("expired"); ok {
		t.Fatal("expired key survived")
	}
}

func TestConcurrent(t *testing.T) {
	c := New[int](16)
	c.Put("hot", 42, time.Minute)
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 1000; j++ {
				if v, ok := c.Get("hot"); !ok || v != 42 {
					t.Error("bad value")
				}
				c.Get("missing")
			}
		}()
	}
	wg.Wait()
	size, hits, misses := c.Stats()
	if size != 1 || hits != 8000 || misses != 8000 {
		t.Fatal(size, hits, misses)
	}
}
