package nativedb

import (
	"context"
	"database/sql"
	"errors"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"sync/atomic"
	"testing"

	"github.com/MoLik-Labs/ALIST/internal/cryptocompat"
)

func TestOpenExistingDoesNotCreate(t *testing.T) {
	path := filepath.Join(t.TempDir(), "missing.db")
	if db, err := OpenExisting(context.Background(), path, "test"); err == nil {
		db.Close()
		t.Fatal("opened missing database")
	}
	if _, err := os.Stat(path); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("missing database was created: %v", err)
	}
	if err := os.WriteFile(path, []byte("not sqlite"), 0600); err != nil {
		t.Fatal(err)
	}
	if db, err := OpenExisting(context.Background(), path, "test"); err == nil {
		db.Close()
		t.Fatal("opened invalid database")
	}
}

func createSchema(t *testing.T) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "data.db")
	db, err := sql.Open("sqlite3", path)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	_, err = db.Exec(`
	CREATE TABLE x_users(id INTEGER PRIMARY KEY, username TEXT UNIQUE, password_hash TEXT, pwd_ts INTEGER, base_path TEXT, identity INTEGER, role_ids_json TEXT, legacy_permission INTEGER, disabled INTEGER, otp_secret TEXT, last_otp_step INTEGER);
	CREATE TABLE x_storages(id INTEGER PRIMARY KEY, mount_path TEXT, order_idx INTEGER, driver TEXT, cache_expiration INTEGER, status TEXT, addition_enc TEXT, remark TEXT, modified INTEGER, disabled INTEGER, enable_sign INTEGER, order_by TEXT, order_direction TEXT, extract_folder TEXT, web_proxy INTEGER, webdav_policy TEXT, down_proxy_url TEXT);
	CREATE TABLE x_shares(share_id TEXT UNIQUE, disabled INTEGER, max_downloads INTEGER, downloads INTEGER, views INTEGER);
	CREATE TABLE x_settings(key TEXT PRIMARY KEY, value TEXT, is_public INTEGER);
	INSERT INTO x_users VALUES(1,'test','hash',1,'/home',0,'[]',0,0,'',-1);
	INSERT INTO x_shares VALUES('test-share',0,7,0,0);
	INSERT INTO x_shares VALUES('disabled',1,0,0,0);
	`)
	if err != nil {
		t.Fatal(err)
	}
	return path
}

func checkConcurrentCounters(t *testing.T, db *Database, userID int64, share string) {
	t.Helper()
	ctx := context.Background()
	var otpOK, shareOK atomic.Int32
	var wg sync.WaitGroup
	for i := 0; i < 32; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ok, err := db.AdvanceOTPStep(ctx, userID, 40)
			if err != nil {
				t.Error(err)
			}
			if ok {
				otpOK.Add(1)
			}
			ok, err = db.ReserveDownload(ctx, share)
			if err != nil {
				t.Error(err)
			}
			if ok {
				shareOK.Add(1)
			}
		}()
	}
	wg.Wait()
	if otpOK.Load() != 1 || shareOK.Load() != 7 {
		t.Fatalf("OTP successes=%d, share successes=%d", otpOK.Load(), shareOK.Load())
	}
}

func TestAtomicCounters(t *testing.T) {
	db, err := OpenExisting(context.Background(), createSchema(t), "secret")
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	checkConcurrentCounters(t, db, 1, "test-share")
	for _, key := range []string{"missing", "disabled"} {
		ok, err := db.ReserveDownload(context.Background(), key)
		if err != nil || ok {
			t.Fatal(key, ok, err)
		}
	}
}

func TestNativeDatabaseInterop(t *testing.T) {
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
	u, err := db.UserByName(ctx, "interop-user")
	if err != nil || u.BasePath != "/home/alice" || len(u.RoleIDs) != 2 || u.RoleIDs[0] != 12 || u.RoleIDs[1] != 42 || u.OTPSecret != "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ" || !cryptocompat.VerifyPassword("fixture-password", u.PasswordHash) {
		t.Fatalf("user read failed: %v", err)
	}
	storages, err := db.Storages(ctx, true)
	if err != nil || len(storages) != 1 || storages[0].MountPath != "/interop" || !storages[0].EnableSign || storages[0].Addition != `{"root_folder_path":"/fixture/private"}` {
		t.Fatalf("storage read failed: %v", err)
	}
	value, public, err := db.Setting(ctx, "interop-key")
	if err != nil || value != "interop-value" || !public {
		t.Fatal(value, public, err)
	}
	checkConcurrentCounters(t, db, u.ID, "interop-share")
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}
	run("inspect")
	wrong, err := OpenExisting(ctx, path, "wrong-secret")
	if err != nil {
		t.Fatal(err)
	}
	defer wrong.Close()
	if _, err := wrong.UserByName(ctx, "interop-user"); err == nil {
		t.Fatal("wrong key accepted for user OTP")
	}
	if _, err := wrong.Storages(ctx, true); err == nil {
		t.Fatal("wrong key accepted for storage")
	}
}
