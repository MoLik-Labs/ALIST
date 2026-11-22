-- Invalidate pending legacy attempts and bind new Google connection attempts to the initiating browser.
ALTER TABLE google_oauth_pending ADD COLUMN browser_hash TEXT;
DELETE FROM google_oauth_pending;
