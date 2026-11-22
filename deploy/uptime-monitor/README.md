# Experimental standalone Render monitor

Instructions and limits: [English](../../docs/guide/monitor-backup.md), [Tiếng Việt](../../docs/guide/vi/monitor-backup.md). This is an original, deliberately narrow implementation informed by inspecting UptimeFlare (`8035521268a54222959eaf720c82f530e755b6bb`); it is not a port or a replacement. No third-party source was copied. No cloud resource has been deployed or modified during development.

The HTTP routes never accept a destination or trigger a check; only cron fetches a single configured `*.onrender.com/ping` (no credentials, no redirects). KV stores a bounded, approximate 24-hour sample history. The page is public and shows only aggregate health, without the configured origin. KV eventual consistency, missed triggers, quotas and network failures can hide or lose samples. It is not an SLA, backup, security boundary, guaranteed keepalive or a production monitoring service.
