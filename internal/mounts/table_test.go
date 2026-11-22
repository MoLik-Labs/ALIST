package mounts

import (
	"reflect"
	"sync"
	"testing"
)

func fixture() []Mount[int] {
	return []Mount[int]{{"/", 1}, {"/local", 2}, {"/local/nested", 3}, {"/local/other/deep", 4}, {"/balanced", 5}, {"/balanced.balance02", 7}, {"/balanced.balance01", 6}, {"/balanced.balance", 8}, {"/hidden.balance/folder", 9}}
}

func TestRoutingAndChildren(t *testing.T) {
	var table Table[int]
	if err := table.Replace(fixture()); err != nil {
		t.Fatal(err)
	}
	for _, tc := range []struct {
		path  string
		value int
		sub   string
	}{
		{"/", 1, "/"}, {"/file", 1, "/file"}, {"/local", 2, "/"},
		{"/local/a", 2, "/a"}, {"/local/nested/a", 3, "/a"},
		{"/locality/a", 1, "/locality/a"}, {"/local/other/deep", 4, "/"},
	} {
		got, sub, ok := table.Match(tc.path)
		if !ok || got.Value != tc.value || sub != tc.sub {
			t.Errorf("Match(%q) = %v %q %v", tc.path, got, sub, ok)
		}
	}
	for _, tc := range []struct {
		path string
		want []string
	}{
		{"/", []string{"balanced", "local"}}, {"/local", []string{"nested", "other"}},
		{"/local/other", []string{"deep"}}, {"/local/nested", nil}, {"/locality", nil},
	} {
		got := table.Children(tc.path)
		if !reflect.DeepEqual(got, tc.want) {
			t.Errorf("Children(%q) = %v, want %v", tc.path, got, tc.want)
		}
	}
	children := table.Children("/")
	children[0] = "modified"
	if table.Children("/")[0] != "balanced" {
		t.Fatal("children exposed internal state")
	}
	for _, path := range []string{"", "relative", "/a/../local", "/local/", "/a\x00"} {
		if _, _, ok := table.Match(path); ok {
			t.Errorf("accepted noncanonical %q", path)
		}
		if got := table.Children(path); got != nil {
			t.Errorf("children for invalid %q: %v", path, got)
		}
	}
}

func TestBalanceAndAtomicReplacement(t *testing.T) {
	var table Table[int]
	if err := table.Replace(fixture()); err != nil {
		t.Fatal(err)
	}
	for _, want := range []int{5, 6, 7, 5, 6, 7} {
		got, sub, ok := table.Match("/balanced/file")
		if !ok || got.Value != want || sub != "/file" {
			t.Fatalf("balance: %v %q %v want %d", got, sub, ok, want)
		}
	}
	for _, invalid := range [][]Mount[int]{{{"/a", 1}, {"/a", 2}}, {{"/a/..", 1}}, {{"/a\x00", 1}}} {
		if table.Replace(invalid) == nil {
			t.Fatal("invalid table accepted")
		}
		if got, _, ok := table.Match("/local"); !ok || got.Value != 2 {
			t.Fatal("failed replace changed table")
		}
	}
}

func TestConcurrentReplace(t *testing.T) {
	var table Table[int]
	if err := table.Replace([]Mount[int]{{"/a", 1}}); err != nil {
		t.Fatal(err)
	}
	var wg sync.WaitGroup
	for i := 0; i < 6; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 2000; j++ {
				m, sub, ok := table.Match("/a/x")
				if !ok || (m.Value != 1 && m.Value != 2) || sub != "/x" {
					t.Errorf("inconsistent match: %v %q %v", m, sub, ok)
					return
				}
				if got := table.Children("/"); len(got) != 1 || got[0] != "a" {
					t.Errorf("bad children: %v", got)
					return
				}
			}
		}()
	}
	for i := 0; i < 2000; i++ {
		if err := table.Replace([]Mount[int]{{"/a", 1 + i%2}}); err != nil {
			t.Fatal(err)
		}
	}
	wg.Wait()
}

func BenchmarkMatch(b *testing.B) {
	var table Table[int]
	_ = table.Replace(fixture())
	b.ReportAllocs()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		table.Match("/local/nested/file")
	}
}
