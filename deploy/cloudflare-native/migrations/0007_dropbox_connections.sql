-- Experimental Dropbox access-token vault; not interchangeable with Google OAuth.
CREATE TABLE dropbox_connections (
  id TEXT PRIMARY KEY,
  owner_id TEXT NOT NULL REFERENCES users(id),
  nonce TEXT NOT NULL,
  ciphertext TEXT NOT NULL,
  created_at INTEGER NOT NULL
);
CREATE INDEX dropbox_connections_owner ON dropbox_connections(owner_id, id);
