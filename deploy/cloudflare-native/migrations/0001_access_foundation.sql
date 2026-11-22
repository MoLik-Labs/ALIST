-- Experimental access foundation. Not connected to HTTP routes or production data.
PRAGMA foreign_keys = ON;
CREATE TABLE users (
  id TEXT PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  disabled INTEGER NOT NULL DEFAULT 0 CHECK (disabled IN (0, 1))
);
CREATE TABLE roles (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE
);
CREATE TABLE user_roles (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  role_id TEXT NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
  PRIMARY KEY (user_id, role_id)
);
-- Resource IDs are opaque application-generated IDs, not provider file paths.
CREATE TABLE resources (
  id TEXT PRIMARY KEY,
  owner_id TEXT NOT NULL REFERENCES users(id),
  visibility TEXT NOT NULL DEFAULT 'private' CHECK (visibility IN ('private', 'public')),
  deleted_at INTEGER
);
CREATE TABLE resource_role_grants (
  resource_id TEXT NOT NULL REFERENCES resources(id) ON DELETE CASCADE,
  role_id TEXT NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
  action TEXT NOT NULL CHECK (action IN ('read', 'write', 'delete', 'share')),
  PRIMARY KEY (resource_id, role_id, action)
);
CREATE TABLE bearer_shares (
  token_hash TEXT PRIMARY KEY,
  resource_id TEXT NOT NULL REFERENCES resources(id) ON DELETE CASCADE,
  created_by TEXT NOT NULL REFERENCES users(id),
  expires_at INTEGER NOT NULL,
  revoked_at INTEGER,
  created_at INTEGER NOT NULL,
  CHECK (expires_at > created_at)
);
CREATE INDEX bearer_shares_resource_id ON bearer_shares(resource_id);
