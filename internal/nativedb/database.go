// Package nativedb implements a small, experimental subset of access to Kova's
// native SQLite schema. It does not initialize or migrate databases and is not a
// replacement for the native Database. Use only with copies for migration tests.
package nativedb

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"net/url"
	"path/filepath"

	"github.com/MoLik-Labs/ALIST/internal/cryptocompat"
	"github.com/MoLik-Labs/ALIST/internal/vpath"
	_ "github.com/mattn/go-sqlite3"
)

var ErrInvalidRecord = errors.New("invalid native database record")

type Database struct {
	db     *sql.DB
	secret string
}

// OpenExisting opens an existing native database without creating or migrating
// it. Only use on a backup/offline copy: concurrent access with the native
// process can leave its in-memory caches stale. The caller owns Close.
// CGO and a C compiler are required by the SQLite driver.
func OpenExisting(ctx context.Context, path, secret string) (*Database, error) {
	if path == "" || path == ":memory:" {
		return nil, errors.New("an existing database file is required")
	}
	absolute, err := filepath.Abs(path)
	if err != nil {
		return nil, err
	}
	u := url.URL{Scheme: "file", Path: absolute}
	q := u.Query()
	q.Set("mode", "rw")
	q.Set("_busy_timeout", "5000")
	q.Set("_foreign_keys", "on")
	u.RawQuery = q.Encode()
	db, err := sql.Open("sqlite3", u.String())
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(4)
	db.SetMaxIdleConns(4)
	if err = db.PingContext(ctx); err != nil {
		db.Close()
		return nil, fmt.Errorf("open native database: %w", err)
	}
	// Check the particular columns this partial adapter uses, not merely the
	// presence of a SQLite file. We intentionally do not change the schema.
	for _, query := range []string{
		"SELECT id, username, password_hash, pwd_ts, base_path, identity, role_ids_json, legacy_permission, disabled, otp_secret, last_otp_step FROM x_users LIMIT 0",
		"SELECT id, mount_path, order_idx, driver, cache_expiration, status, addition_enc, remark, modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, webdav_policy, down_proxy_url FROM x_storages LIMIT 0",
		"SELECT share_id, disabled, max_downloads, downloads, views FROM x_shares LIMIT 0",
		"SELECT key, value, is_public FROM x_settings LIMIT 0",
	} {
		rows, err := db.QueryContext(ctx, query)
		if err != nil {
			db.Close()
			return nil, fmt.Errorf("unsupported native schema: %w", err)
		}
		if err := rows.Close(); err != nil {
			db.Close()
			return nil, err
		}
	}
	return &Database{db: db, secret: secret}, nil
}

func (d *Database) Close() error { return d.db.Close() }

type User struct {
	ID                     int64
	Username, PasswordHash string
	PasswordTimestamp      int64
	BasePath               string
	Identity               int
	RoleIDs                []int64
	LegacyPermission       int32
	Disabled               bool
	OTPSecret              string
	LastOTPStep            int64
}

func (d *Database) UserByName(ctx context.Context, name string) (User, error) {
	var u User
	var roles, encryptedOTP string
	err := d.db.QueryRowContext(ctx, "SELECT id, username, password_hash, pwd_ts, base_path, identity, role_ids_json, legacy_permission, disabled, otp_secret, last_otp_step FROM x_users WHERE username = ?", name).Scan(&u.ID, &u.Username, &u.PasswordHash, &u.PasswordTimestamp, &u.BasePath, &u.Identity, &roles, &u.LegacyPermission, &u.Disabled, &encryptedOTP, &u.LastOTPStep)
	if err != nil {
		return User{}, err
	}
	if !vpath.IsClean(u.BasePath) {
		return User{}, fmt.Errorf("user base path: %w", ErrInvalidRecord)
	}
	if err = json.Unmarshal([]byte(roles), &u.RoleIDs); err != nil || u.RoleIDs == nil {
		return User{}, fmt.Errorf("user role IDs: %w", ErrInvalidRecord)
	}
	u.OTPSecret, err = cryptocompat.DecryptVault(d.secret, encryptedOTP)
	if err != nil {
		return User{}, fmt.Errorf("decrypt user OTP: %w", err)
	}
	return u, nil
}

type Storage struct {
	ID                                     int64
	MountPath                              string
	Order                                  int
	Driver                                 string
	CacheExpiration                        int
	Status, Addition, Remark               string
	Modified                               int64
	Disabled, EnableSign                   bool
	OrderBy, OrderDirection, ExtractFolder string
	WebProxy                               bool
	WebDAVPolicy, DownProxyURL             string
}

func (d *Database) Storages(ctx context.Context, enabledOnly bool) ([]Storage, error) {
	query := "SELECT id, mount_path, order_idx, driver, cache_expiration, status, addition_enc, remark, modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, webdav_policy, down_proxy_url FROM x_storages"
	if enabledOnly {
		query += " WHERE disabled = 0"
	}
	query += " ORDER BY order_idx, id"
	rows, err := d.db.QueryContext(ctx, query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []Storage
	for rows.Next() {
		var s Storage
		var encrypted string
		if err := rows.Scan(&s.ID, &s.MountPath, &s.Order, &s.Driver, &s.CacheExpiration, &s.Status, &encrypted, &s.Remark, &s.Modified, &s.Disabled, &s.EnableSign, &s.OrderBy, &s.OrderDirection, &s.ExtractFolder, &s.WebProxy, &s.WebDAVPolicy, &s.DownProxyURL); err != nil {
			return nil, err
		}
		if !vpath.IsClean(s.MountPath) {
			return nil, fmt.Errorf("storage mount: %w", ErrInvalidRecord)
		}
		s.Addition, err = cryptocompat.DecryptVault(d.secret, encrypted)
		if err != nil {
			return nil, fmt.Errorf("decrypt storage %d: %w", s.ID, err)
		}
		out = append(out, s)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return out, nil
}

func (d *Database) Setting(ctx context.Context, key string) (value string, public bool, err error) {
	err = d.db.QueryRowContext(ctx, "SELECT value, is_public FROM x_settings WHERE key = ?", key).Scan(&value, &public)
	return
}

// AdvanceOTPStep atomically consumes a TOTP step, matching the native SQL.
// The caller must authenticate the user and validate the TOTP before this call.
func (d *Database) AdvanceOTPStep(ctx context.Context, userID, step int64) (bool, error) {
	result, err := d.db.ExecContext(ctx, "UPDATE x_users SET last_otp_step = ? WHERE id = ? AND last_otp_step < ?", step, userID, step)
	if err != nil {
		return false, err
	}
	count, err := result.RowsAffected()
	return count == 1, err
}

// ReserveDownload atomically increments the download count if the share is
// enabled and has quota left. The caller must separately verify the share's
// expiry, password and authorization; this function is not a complete check.
func (d *Database) ReserveDownload(ctx context.Context, shareID string) (bool, error) {
	result, err := d.db.ExecContext(ctx, "UPDATE x_shares SET downloads = downloads + 1 WHERE share_id = ? AND disabled = 0 AND (max_downloads = 0 OR downloads < max_downloads)", shareID)
	if err != nil {
		return false, err
	}
	count, err := result.RowsAffected()
	return count == 1, err
}
