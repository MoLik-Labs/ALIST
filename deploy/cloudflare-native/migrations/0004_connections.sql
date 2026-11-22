-- Experimental single-owner credential records; no HTTP/API integration yet.
CREATE TABLE provider_connections (
  id TEXT PRIMARY KEY,
  owner_id TEXT NOT NULL REFERENCES users(id),
  provider TEXT NOT NULL CHECK (provider IN ('google_drive')),
  vault_version INTEGER NOT NULL CHECK (vault_version = 1),
  nonce TEXT NOT NULL,
  ciphertext TEXT NOT NULL,
  created_at INTEGER NOT NULL
);
CREATE INDEX provider_connections_owner ON provider_connections(owner_id);
