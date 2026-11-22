//go:build sqlite_fts5 || fts5

package nativedb

import (
	"context"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

func TestSearchIndex(t *testing.T) {
	ctx := context.Background()
	db, err := OpenExisting(ctx, createSchema(t), "secret")
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	if enabled, err := db.FTSAvailable(ctx); err != nil || !enabled {
		t.Fatal(enabled, err)
	}
	_, err = db.db.ExecContext(ctx, `CREATE VIRTUAL TABLE x_search_fts USING fts5(name, parent_path, full_path UNINDEXED, is_dir UNINDEXED, size UNINDEXED, file_type UNINDEXED, tokenize='unicode61 remove_diacritics 2')`)
	if err != nil {
		t.Fatal(err)
	}
	if err = db.IndexDirectory(ctx, "/docs", []SearchEntry{{Name: "Café architecture.pdf", Size: 42}, {Name: "Architecture folder", IsDir: true}}); err != nil {
		t.Fatal(err)
	}
	if err = db.IndexDirectory(ctx, "/docs2", []SearchEntry{{Name: "architecture.txt"}}); err != nil {
		t.Fatal(err)
	}
	hits, total, err := db.Search(ctx, "arch", "/docs", 0, 1, 1)
	if err != nil || len(hits) != 1 || total != 2 {
		t.Fatal(hits, total, err)
	}
	hits, total, err = db.Search(ctx, "cafe arch", "/docs", 2, 1, 20)
	if err != nil || total != 1 || len(hits) != 1 || hits[0].Name != "Café architecture.pdf" {
		t.Fatal(hits, total, err)
	}
	hits, total, err = db.Search(ctx, "architecture", "/docs", 1, 1, 20)
	if err != nil || total != 1 || len(hits) != 1 || !hits[0].IsDir {
		t.Fatal(hits, total, err)
	}
	hits, total, err = db.Search(ctx, "architecture", "/docs", 0, int(^uint(0)>>1), 1)
	if err != nil || total != 2 || len(hits) != 0 {
		t.Fatal(hits, total, err)
	}
	if err = db.IndexDirectory(ctx, "/docs", []SearchEntry{{Name: "../bad"}}); !errors.Is(err, ErrInvalidRecord) {
		t.Fatal("accepted invalid name", err)
	}
	if count, err := db.CountIndex(ctx); err != nil || count != 3 {
		t.Fatal("failed replacement changed index", count, err)
	}
	if err := db.RemoveIndexPrefix(ctx, "/docs"); err != nil {
		t.Fatal(err)
	}
	if count, err := db.CountIndex(ctx); err != nil || count != 1 {
		t.Fatal("sibling deleted", count, err)
	}
	if _, _, err := db.Search(ctx, "foo", "/bad/../", 0, 1, 1); !errors.Is(err, ErrInvalidRecord) {
		t.Fatal("accepted invalid scope", err)
	}
}

func TestNativeSearchInterop(t *testing.T) {
	fixture := os.Getenv("KOVA_NATIVE_DB_FIXTURE")
	if fixture == "" {
		t.Skip("set KOVA_NATIVE_DB_FIXTURE for native interoperability")
	}
	path := filepath.Join(t.TempDir(), "native.db")
	run := func(mode string) {
		t.Helper()
		out, err := exec.Command(fixture, mode, path).CombinedOutput()
		if err != nil {
			t.Fatalf("native %s: %v: %s", mode, err, out)
		}
	}
	run("create")
	ctx := context.Background()
	db, err := OpenExisting(ctx, path, "fixture-master-secret")
	if err != nil {
		t.Fatal(err)
	}
	hits, total, err := db.Search(ctx, "arch", "/interop", 0, 1, 50)
	if err != nil || total != 1 || len(hits) != 1 || hits[0].Name != "Architecture.pdf" || hits[0].Size != 42 {
		t.Fatal(hits, total, err)
	}
	if err := db.IndexDirectory(ctx, "/interop/nested", []SearchEntry{{Name: "Blueprint.txt", Size: 101}}); err != nil {
		t.Fatal(err)
	}
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}
	run("inspect-search")
}
