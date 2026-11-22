-- Experimental pre-provisioned Google identities; email addresses are not IDs.
CREATE TABLE external_identities (
  provider TEXT NOT NULL CHECK (provider = 'google'),
  subject TEXT NOT NULL,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  PRIMARY KEY (provider, subject)
);
CREATE INDEX external_identities_user_id ON external_identities(user_id);
CREATE TABLE oidc_login_pending (
  state_hash TEXT PRIMARY KEY,
  browser_hash TEXT NOT NULL,
  nonce TEXT NOT NULL,
  verifier_nonce TEXT NOT NULL,
  verifier_ciphertext TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  CHECK (expires_at > created_at)
);
CREATE INDEX oidc_login_pending_expires ON oidc_login_pending(expires_at);
