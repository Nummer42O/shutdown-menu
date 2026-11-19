#!/usr/bin/env bash
# Start a private nested GNOME Shell Wayland session and verify that the
# application appears without GTK criticals/warnings.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

app="${1:?'missing application binary'}"
artifactDir="${2:?'missing artifact directory'}"
schemaDir="${3:-}"

# Always use a private session bus. The user's desktop session bus already has
# a GNOME Shell, which would make the nested shell refuse to start.
if [[ -z "${SHUTDOWN_MENU_TEST_PRIVATE_BUS:-}" ]]; then
  need_cmd dbus-run-session
  export SHUTDOWN_MENU_TEST_PRIVATE_BUS=1
  exec dbus-run-session -- "$0" "$@"
fi

need_file "$app"
need_cmd Xephyr
need_cmd gnome-shell
need_cmd import
need_cmd convert
need_cmd compare

mkdir -p "$artifactDir"

# Xephyr needs an X11 display to nest in.
if [[ -z "${DISPLAY:-}" ]]; then
  need_cmd Xvfb
  start_bg Xvfb :98 -screen 1280x800x24
  export DISPLAY=:98
  sleep 1
fi

nestedDisplay="${SHUTDOWN_MENU_TEST_DISPLAY:-}"
if [[ -z "$nestedDisplay" ]]; then
  for candidate in 99 98 97 96 95; do
    if [[ ! -e "/tmp/.X11-unix/X${candidate}" ]]; then
      nestedDisplay=":${candidate}"
      break
    fi
  done
fi
[[ -n "$nestedDisplay" ]] || die "no free X11 display for Xephyr"

runtimeDir="$(make_temp_dir wayland-runtime)"
chmod 700 "$runtimeDir"
export XDG_RUNTIME_DIR="$runtimeDir"
unset WAYLAND_DISPLAY || true

appPid=""
function cleanup {
  if [[ -n "$appPid" ]] && kill -0 "$appPid" 2>/dev/null; then
    kill -TERM "$appPid" 2>/dev/null || true
  fi
  kill_bg
  rm -rf "$runtimeDir"
}
trap cleanup EXIT

log "starting Xephyr on ${nestedDisplay}"
start_bg Xephyr "$nestedDisplay" -screen 1280x800 -noreset
sleep 2

log "starting nested GNOME Shell (Wayland)"
start_bg env DISPLAY="$nestedDisplay" gnome-shell \
  --nested --wayland --no-x11 --wayland-display=shutdown-menu-test \
  >"${artifactDir}/gnome-shell.log" 2>&1

if ! wait_until 30 "Wayland socket" test -S "${runtimeDir}/shutdown-menu-test"; then
  tail -40 "${artifactDir}/gnome-shell.log" >&2 || true
  skip "nested GNOME Shell did not create a Wayland socket"
fi

log "waiting for the nested desktop"
baselineReady=0
for _ in $(seq 1 20); do
  import -display "$nestedDisplay" -window root "${artifactDir}/before.png" 2>/dev/null || true
  if [[ -f "${artifactDir}/before.png" ]]; then
    mean="$(convert "${artifactDir}/before.png" -colorspace gray -format '%[fx:mean]' info: 2>/dev/null || echo 0)"
    if awk -v m="$mean" 'BEGIN { exit !(m > 0.005) }'; then
      baselineReady=1
      break
    fi
  fi
  sleep 1
done
if [[ "$baselineReady" != "1" ]]; then
  skip "nested GNOME Shell did not render a desktop"
fi
log "taking baseline screenshot"
import -display "$nestedDisplay" -window root "${artifactDir}/before.png"

appLog="${artifactDir}/app.log"
log "starting shutdown-menu under Wayland"
# shellcheck disable=2054
appEnv=(
  env -u DISPLAY
  WAYLAND_DISPLAY=shutdown-menu-test
  GDK_BACKEND=wayland
  G_DEBUG=fatal-criticals,fatal-warnings
)
if [[ -n "$schemaDir" ]]; then
  appEnv+=(GSETTINGS_SCHEMA_DIR="$schemaDir")
fi
start_bg "${appEnv[@]}" "$app" >"$appLog" 2>&1
appPid="$lastBgPid"

crop="400x300+200+150"
beforeCrop="${artifactDir}/before-crop.png"
afterCrop="${artifactDir}/after-crop.png"
convert "${artifactDir}/before.png" -crop "$crop" +repage "$beforeCrop"

changed=0
for _ in $(seq 1 30); do
  if ! kill -0 "$appPid" 2>/dev/null; then
    log "app exited before rendering"
    cat "$appLog" >&2 || true
    break
  fi
  if import -display "$nestedDisplay" -window root "${artifactDir}/current.png" 2>/dev/null; then
    convert "${artifactDir}/current.png" -crop "$crop" +repage "$afterCrop"
    metric="$(compare -metric AE "$beforeCrop" "$afterCrop" null: 2>&1 || true)"
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
  die "Wayland smoke test failed: application did not render"
fi

import -display "$nestedDisplay" -window root "${artifactDir}/after.png"
if grep -E 'CRITICAL|WARNING|ERROR' "$appLog" >/dev/null 2>&1; then
  cat "$appLog" >&2 || true
  die "Wayland smoke test found warnings/criticals in the application log"
fi

log "Wayland smoke test passed; artifacts in ${artifactDir}"
