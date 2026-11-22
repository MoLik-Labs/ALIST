#!/bin/sh
# Requires Docker; starts disposable containers, never connects to cloud accounts.
set -eu
image=${1:-kova-native:ci}
secret='ci-only-admin-password-never-use-in-production'
container=''
cleanup() {
    if [ -n "$container" ]; then docker rm -f "$container" >/dev/null 2>&1 || :; fi
}
trap cleanup EXIT INT TERM

run_case() {
    mode=$1
    shift
    container=$(docker run -d "$@" -e PORT=5244 -e "ALIST_ADMIN_PASSWORD=$secret" "$image")
    ready=0
    for attempt in $(seq 1 90); do
        if docker exec "$container" curl -fsS http://127.0.0.1:5244/ping >/dev/null 2>&1; then
            ready=1
            break
        fi
        if [ "$(docker inspect -f '{{.State.Running}}' "$container")" != true ]; then break; fi
        sleep 1
    done
    if [ "$ready" != 1 ]; then
        echo "Container $mode did not become ready" >&2
        docker logs "$container" >&2
        exit 1
    fi
    if docker logs "$container" 2>&1 | grep -F "$secret" >/dev/null; then
        echo 'Bootstrap secret leaked into logs' >&2
        exit 1
    fi
    if [ "$mode" = heroku ]; then
        if ! docker logs "$container" 2>&1 | grep -F 'data is ephemeral' >/dev/null; then
            echo 'Missing ephemeral storage warning' >&2
            exit 1
        fi
    fi
    docker rm -f "$container" >/dev/null
    container=''
}

run_case default
run_case heroku --user 10001:10001 -e DYNO=web.1
printf '%s\n' 'Native Docker default and non-root Heroku-style smoke checks passed'
