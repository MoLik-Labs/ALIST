-- Experimental, short-lived Dropbox OAuth attempts; no production use.
CREATE TABLE dropbox_oauth_pending (
  state_hash TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  session_hash TEXT NOT NULL REFERENCES sessions(token_hash) ON DELETE CASCADE,
  browser_hash TEXT NOT NULL,
  verifier_nonce TEXT NOT NULL,
  verifier_ciphertext TEXT NOT NULL,
  expires_at INTEGER NOT NULL,
  created_at INTEGER NOT NULL,
  CHECK (expires_at > created_at)
);
CREATE INDEX dropbox_oauth_pending_expires ON dropbox_oauth_pending(expires_at);
