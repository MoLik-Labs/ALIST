#!/bin/sh
set -eu

PUID="${PUID:-0}"
PGID="${PGID:-0}"
UMASK="${UMASK:-022}"

umask "$UMASK"
mkdir -p /opt/alist/data

if [ "$PUID" != "0" ] || [ "$PGID" != "0" ]; then
    if ! getent group alist >/dev/null 2>&1; then
        groupadd -o -g "$PGID" alist
    fi
    if ! id -u alist >/dev/null 2>&1; then
        useradd -o -u "$PUID" -g "$PGID" -d /opt/alist -s /usr/sbin/nologin alist
    fi
    chown -R "${PUID}:${PGID}" /opt/alist/data
    exec gosu alist /opt/alist/alist server --data /opt/alist/data "$@"
fi

exec /opt/alist/alist server --data /opt/alist/data "$@"
