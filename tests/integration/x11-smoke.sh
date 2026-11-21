#!/usr/bin/env bash
# Start a fresh X11 display and verify that the application maps a window.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

app="${1:?'missing application binary'}"
artifactDir="${2:?'missing artifact directory'}"
schemaDir="${3:-}"

if [[ -z "${SHUTDOWN_MENU_TEST_PRIVATE_BUS:-}" ]]; then
  need_cmd dbus-run-session
  export SHUTDOWN_MENU_TEST_PRIVATE_BUS=1
  exec dbus-run-session -- "$0" "$@"
fi

need_file "$app"
need_cmd Xephyr
need_cmd import
need_cmd compare
need_cmd convert

if [[ -z "${SHUTDOWN_MENU_X11_WM:-}" ]]; then
  skip "set SHUTDOWN_MENU_X11_WM to an X11 window manager; Xephyr alone does not manage windows"
fi
if [[ "$SHUTDOWN_MENU_X11_WM" == */* ]]; then
  need_file "$SHUTDOWN_MENU_X11_WM"
else
  need_cmd "$SHUTDOWN_MENU_X11_WM"
fi

mkdir -p "$artifactDir"

if [[ -z "${DISPLAY:-}" ]]; then
  need_cmd Xvfb
  start_bg Xvfb :97 -screen 1280x800x24
  export DISPLAY=:97
  sleep 1
fi

nestedDisplay="${SHUTDOWN_MENU_TEST_X11_DISPLAY:-}"
if [[ -z "$nestedDisplay" ]]; then
  for candidate in 96 95 94 93; do
    if [[ ! -e "/tmp/.X11-unix/X${candidate}" ]]; then
      nestedDisplay=":${candidate}"
      break
    fi
  done
fi
[[ -n "$nestedDisplay" ]] || die "no free X11 display for Xephyr"

appPid=""
function cleanup {
  if [[ -n "$appPid" ]] && kill -0 "$appPid" 2>/dev/null; then
    kill -TERM "$appPid" 2>/dev/null || true
  fi
  kill_bg
}
trap cleanup EXIT

log "starting Xephyr on ${nestedDisplay}"
start_bg Xephyr "$nestedDisplay" -screen 1280x800 -noreset
sleep 2

if [[ -n "${SHUTDOWN_MENU_X11_WM:-}" ]]; then
  log "starting optional window manager ${SHUTDOWN_MENU_X11_WM}"
  start_bg env DISPLAY="$nestedDisplay" "$SHUTDOWN_MENU_X11_WM"
  sleep 1
fi

import -display "$nestedDisplay" -window root "${artifactDir}/before.png"

appLog="${artifactDir}/app.log"
# shellcheck disable=2054
appEnv=(
  env DISPLAY="$nestedDisplay"
  GDK_BACKEND=x11
  GTK_A11Y=test
  G_DEBUG=fatal-criticals,fatal-warnings
)
if [[ -n "$schemaDir" ]]; then
  appEnv+=(GSETTINGS_SCHEMA_DIR="$schemaDir")
fi
log "starting shutdown-menu under X11"
start_bg "${appEnv[@]}" "$app" >"$appLog" 2>&1
appPid="$lastBgPid"

changed=0
for _ in $(seq 1 20); do
  if ! kill -0 "$appPid" 2>/dev/null; then
    log "app exited before rendering"
    cat "$appLog" >&2 || true
    break
  fi
  if import -display "$nestedDisplay" -window root "${artifactDir}/current.png" 2>/dev/null; then
    metric="$(compare -metric AE "${artifactDir}/before.png" "${artifactDir}/current.png" null: 2>&1 || true)"
    if [[ "$metric" =~ ^[0-9]+$ ]] && (( metric > 1000 )); then
      changed=1
      break
    fi
  fi
  sleep 1
done

if [[ "$changed" != "1" ]]; then
  log "no visible application window detected"
  cat "$appLog" >&2 || true
  die "X11 smoke test failed: application did not render"
fi

import -display "$nestedDisplay" -window root "${artifactDir}/after.png"
if grep -E 'CRITICAL|WARNING|ERROR' "$appLog" >/dev/null 2>&1; then
  cat "$appLog" >&2 || true
  die "X11 smoke test found warnings/criticals in the application log"
fi

log "X11 smoke test passed; artifacts in ${artifactDir}"
