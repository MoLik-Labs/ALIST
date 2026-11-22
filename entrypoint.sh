#!/bin/sh
set -eu

PUID="${PUID:-0}"
PGID="${PGID:-0}"
UMASK="${UMASK:-027}"

case "$PUID:$PGID" in
    *[!0-9:]*|:*|*:|*:*:*) echo "PUID and PGID must be non-negative integers" >&2; exit 64 ;;
esac
case "$UMASK" in
    ''|*[!0-7]*) echo "UMASK must contain octal digits only" >&2; exit 64 ;;
esac

umask "$UMASK"
# Container platforms may supply the image command as the first argument.
if [ "${1:-}" = "server" ]; then
    shift
fi

# Heroku runs the image as an arbitrary non-root UID, with an ephemeral root
# filesystem. For a demo, create a private temporary directory that this UID can
# write. Explicit configuration always wins; never claim temporary data is durable.
managed_data=0
if [ -n "${ALIST_DATA_DIR:-}" ]; then
    data_dir=$ALIST_DATA_DIR
    if [ "$data_dir" = /opt/alist/data ]; then managed_data=1; fi
elif [ -n "${DYNO:-}" ]; then
    data_dir=$(mktemp -d "${TMPDIR:-/tmp}/kova.XXXXXXXX")
    managed_data=1
    echo "WARNING: Heroku demo data is ephemeral and will be lost on restart or redeploy." >&2
else
    data_dir=/opt/alist/data
    managed_data=1
fi

mkdir -p "$data_dir"

# An orchestrator may force a non-root UID regardless of the Dockerfile.
# Do not attempt useradd/chown as that user, and do not silently fall back to
# ephemeral storage on platforms where a persistent volume may be expected.
if [ "$(id -u)" != "0" ]; then
    if [ ! -w "$data_dir" ]; then
        echo "Data directory is not writable by the current container user: $data_dir" >&2
        exit 73
    fi
    exec /opt/alist/alist server --data "$data_dir" "$@"
fi

if [ "$PUID" != "0" ] || [ "$PGID" != "0" ]; then
    if ! getent group alist >/dev/null 2>&1; then
        groupadd -o -g "$PGID" alist
    fi
    if ! id -u alist >/dev/null 2>&1; then
        useradd -o -u "$PUID" -g "$PGID" -d /opt/alist -s /usr/sbin/nologin alist
    fi
    if [ "$managed_data" = 1 ]; then
        chown -R "${PUID}:${PGID}" "$data_dir"
    elif ! gosu alist test -w "$data_dir"; then
        echo "Custom data directory must be writable by the configured container user: $data_dir" >&2
        exit 73
    fi
    exec gosu alist /opt/alist/alist server --data "$data_dir" "$@"
fi

exec /opt/alist/alist server --data "$data_dir" "$@"
