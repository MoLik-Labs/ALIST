package nativedb

import (
	"context"
	"fmt"
	"strings"

	"github.com/MoLik-Labs/ALIST/internal/vpath"
)

// SearchEntry is a file or directory indexed in the native FTS5 table.
type SearchEntry struct {
	Name  string
	IsDir bool
	Size  int64
	Type  int
}

type SearchHit struct {
	Parent string
	SearchEntry
}

// FTSAvailable reports whether the SQLite library backing this Go build has
// FTS5. For the bundled SQLite driver, build with -tags sqlite_fts5.
func (d *Database) FTSAvailable(ctx context.Context) (bool, error) {
	var enabled int
	err := d.db.QueryRowContext(ctx, "SELECT sqlite_compileoption_used('ENABLE_FTS5')").Scan(&enabled)
	return enabled != 0, err
}

// IndexDirectory replaces one directory's indexed entries in a transaction.
// Each name must be a single safe filename. The caller controls authorization,
// storage traversal and freshness; this function does not inspect the backend.
func (d *Database) IndexDirectory(ctx context.Context, parent string, entries []SearchEntry) (err error) {
	if !vpath.IsClean(parent) {
		return ErrInvalidRecord
	}
	for _, entry := range entries {
		if !vpath.ValidFilename(entry.Name) {
			return fmt.Errorf("invalid indexed filename: %w", ErrInvalidRecord)
		}
	}
	tx, err := d.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()
	if _, err = tx.ExecContext(ctx, "DELETE FROM x_search_fts WHERE parent_path = ?", parent); err != nil {
		return err
	}
	stmt, err := tx.PrepareContext(ctx, "INSERT INTO x_search_fts(name, parent_path, full_path, is_dir, size, file_type) VALUES(?,?,?,?,?,?)")
	if err != nil {
		return err
	}
	defer stmt.Close()
	for _, entry := range entries {
		full, ok := vpath.Join(parent, entry.Name)
		if !ok {
			return ErrInvalidRecord
		}
		if _, err = stmt.ExecContext(ctx, entry.Name, parent, full, entry.IsDir, entry.Size, entry.Type); err != nil {
			return err
		}
	}
	return tx.Commit()
}

// RemoveIndexPrefix deletes entries for a path and its descendants without
// matching siblings that share only a string prefix.
func (d *Database) RemoveIndexPrefix(ctx context.Context, path string) error {
	if !vpath.IsClean(path) {
		return ErrInvalidRecord
	}
	if path == "/" {
		_, err := d.db.ExecContext(ctx, "DELETE FROM x_search_fts")
		return err
	}
	_, err := d.db.ExecContext(ctx, "DELETE FROM x_search_fts WHERE full_path = ? OR (full_path > ? AND full_path < ?)", path, path+"/", path+"0")
	return err
}

func (d *Database) CountIndex(ctx context.Context) (int64, error) {
	var count int64
	err := d.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM x_search_fts").Scan(&count)
	return count, err
}

// Search matches native Kova's prefix-token queries and 1000-candidate limit.
// scopeMode is 0 for all, 1 for folders, 2 for files. The caller must apply
// user/metadata permissions to every hit; a scope alone is not authorization.
func (d *Database) Search(ctx context.Context, keywords, scope string, scopeMode, page, perPage int) ([]SearchHit, int64, error) {
	if !vpath.IsClean(scope) {
		return nil, 0, ErrInvalidRecord
	}
	keywords = strings.Trim(keywords, " \t\r\n")
	var tokens []string
	for _, tok := range strings.Split(keywords, " ") {
		var b strings.Builder
		for i := 0; i < len(tok); i++ {
			if tok[i] != '"' && tok[i] >= 0x20 {
				b.WriteByte(tok[i])
			}
		}
		if b.Len() != 0 {
			tokens = append(tokens, `name:"`+b.String()+`"*`)
		}
	}
	if len(tokens) == 0 {
		return nil, 0, nil
	}
	if page < 1 {
		page = 1
	}
	if perPage < 1 || perPage > 500 {
		perPage = 50
	}
	// Avoid overflowing int on extreme client-supplied page numbers.
	if page > 1001 {
		page = 1001
	}
	offset := (page - 1) * perPage
	rows, err := d.db.QueryContext(ctx, "SELECT parent_path, name, is_dir, size, file_type FROM x_search_fts WHERE x_search_fts MATCH ? ORDER BY rank LIMIT 1000", strings.Join(tokens, " AND "))
	if err != nil {
		return nil, 0, err
	}
	defer rows.Close()
	var hits []SearchHit
	var total int64
	for rows.Next() {
		var hit SearchHit
		if err := rows.Scan(&hit.Parent, &hit.Name, &hit.IsDir, &hit.Size, &hit.Type); err != nil {
			return nil, 0, err
		}
		if !vpath.IsClean(hit.Parent) || !vpath.ValidFilename(hit.Name) {
			return nil, 0, fmt.Errorf("invalid search record: %w", ErrInvalidRecord)
		}
		if !vpath.IsSubpath(scope, hit.Parent) || (scopeMode == 1 && !hit.IsDir) || (scopeMode == 2 && hit.IsDir) {
			continue
		}
		if total >= int64(offset) && total < int64(offset+perPage) {
			hits = append(hits, hit)
		}
		total++
	}
	if err := rows.Err(); err != nil {
		return nil, 0, err
	}
	return hits, total, nil
}
