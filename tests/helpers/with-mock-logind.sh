#!/usr/bin/env bash
# Run a command with a private D-Bus bus and a mock org.freedesktop.login1.
#
# Usage:
#   with-mock-logind.sh --record FILE [--python PYTHON] [--can-result VALUE] \
#     [--exit-after N] -- COMMAND [ARG...]
#
# The command runs with DBUS_SYSTEM_BUS_ADDRESS pointing at the private bus.
# The mock records every PowerOff/Reboot call as JSON Lines in FILE. This
# helper never talks to the real system bus.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/common.sh"

# Re-exec inside a fresh session bus unless the caller already supplied one.
if [[ -z "${SHUTDOWN_MENU_TEST_PRIVATE_BUS:-}" ]]; then
  need_cmd dbus-run-session
  export SHUTDOWN_MENU_TEST_PRIVATE_BUS=1
  export SHUTDOWN_MENU_TEST_MOCK_BUS=1
  exec dbus-run-session -- "$0" "$@"
fi

python="${PYTHON:-python3}"
recordFile=""
canResult=""
exitAfter=""

function usage {
  cat >&2 <<EOF
Usage: $0 --record FILE [--python PYTHON] [--can-result yes|no|na|challenge]
          [--exit-after N] -- COMMAND [ARG...]
EOF
}

while (( $# )); do
  case "$1" in
    --record)
      [[ $# -ge 2 ]] || { usage; die "missing value for --record"; }
      recordFile="$2"
      shift 2
      ;;
    --python)
      [[ $# -ge 2 ]] || { usage; die "missing value for --python"; }
      python="$2"
      shift 2
      ;;
    --can-result)
      [[ $# -ge 2 ]] || { usage; die "missing value for --can-result"; }
      canResult="$2"
      shift 2
      ;;
    --exit-after)
      [[ $# -ge 2 ]] || { usage; die "missing value for --exit-after"; }
      exitAfter="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    *)
      usage
      die "unknown option: $1"
      ;;
  esac
done

[[ -n "$recordFile" ]] || { usage; die "--record is required"; }
(( $# > 0 )) || { usage; die "missing command"; }

need_cmd "$python"
need_file "${scriptDir}/mock-logind.py"

"$python" - <<'PY' || skip "python3-gi is required for the mock logind service"
import sys
try:
    from gi.repository import Gio, GLib  # noqa: F401
except ImportError:
    sys.exit(1)
PY

if [[ "${SHUTDOWN_MENU_TEST_MOCK_BUS:-}" == "1" ]]; then
  # Do not allow a real DBUS_SYSTEM_BUS_ADDRESS inherited from the environment
  # to leak into a test that explicitly asked for an isolated bus.
  [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]] \
    || die "no private D-Bus address is available"
  export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
elif [[ -z "${DBUS_SYSTEM_BUS_ADDRESS:-}" ]]; then
  [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]] \
    || die "no private D-Bus address is available"
  export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
fi

mkdir -p "$(dirname -- "$recordFile")"
: > "$recordFile"

tmpDir="$(make_temp_dir mock-logind)"
readyFile="${tmpDir}/ready"
mockLog="${tmpDir}/mock.log"

function cleanup {
  kill_bg
  rm -rf "$tmpDir"
}
trap cleanup EXIT

mockArgs=(
  --bus-address "$DBUS_SYSTEM_BUS_ADDRESS"
  --record "$recordFile"
  --ready-file "$readyFile"
)
if [[ -n "$canResult" ]]; then
  mockArgs+=(--can-result "$canResult")
fi
if [[ -n "$exitAfter" ]]; then
  mockArgs+=(--exit-after "$exitAfter")
fi

log "starting mock logind on private bus"
start_bg "$python" "${scriptDir}/mock-logind.py" "${mockArgs[@]}" \
  >"$mockLog" 2>&1

if ! wait_until 10 "mock logind" test -f "$readyFile"; then
  cat "$mockLog" >&2 || true
  die "mock logind did not acquire org.freedesktop.login1"
fi

log "running $*"
set +e
"$@"
status=$?
set -e

exit "$status"
