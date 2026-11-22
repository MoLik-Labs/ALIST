-- Global control-plane privileges are explicit role grants, never role-name based.
-- Experimental: no public management endpoints consume these permissions yet.
CREATE TABLE role_permissions (
  role_id TEXT NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
  permission TEXT NOT NULL CHECK (permission IN ('manage_users')),
  PRIMARY KEY (role_id, permission)
);
