#!/usr/bin/env bash
# Exercise the mock logind helper itself. This does not start the GUI; it
# verifies that the private bus, service name and call recording work.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

python="${1:?'missing python interpreter'}"
gdbus="${2:?'missing gdbus executable'}"
artifactDir="${3:?'missing artifact directory'}"

need_file "${scriptDir}/../helpers/with-mock-logind.sh"
need_cmd "$python"
need_cmd "$gdbus"

mkdir -p "$artifactDir"
recordFile="${artifactDir}/calls.jsonl"
rm -f "$recordFile"

log "calling mock logind through a private bus"
"${scriptDir}/../helpers/with-mock-logind.sh" \
  --record "$recordFile" \
  --python "$python" \
  -- bash -c '
    set -euo pipefail
    gdbus="$1"

    "$gdbus" call \
      --system \
      --dest org.freedesktop.login1 \
      --object-path /org/freedesktop/login1 \
      --method org.freedesktop.login1.Manager.PowerOff \
      false

    "$gdbus" call \
      --system \
      --dest org.freedesktop.login1 \
      --object-path /org/freedesktop/login1 \
      --method org.freedesktop.login1.Manager.Reboot \
      true

    result="$("$gdbus" call \
      --system \
      --dest org.freedesktop.login1 \
      --object-path /org/freedesktop/login1 \
      --method org.freedesktop.login1.Manager.CanPowerOff)"
    [[ "$result" == *yes* ]] || {
      printf "unexpected CanPowerOff result: %s\n" "$result" >&2
      exit 1
    }
  ' bash "$gdbus"

need_file "$recordFile"
"$python" - "$recordFile" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as record_file:
    calls = [json.loads(line) for line in record_file if line.strip()]

if [call["method"] for call in calls] != ["PowerOff", "Reboot"]:
    raise SystemExit("unexpected mock call order: %r" % calls)
if calls[0]["interactive"] is not False:
    raise SystemExit("PowerOff interactive flag was not false: %r" % calls[0])
if calls[1]["interactive"] is not True:
    raise SystemExit("Reboot interactive flag was not true: %r" % calls[1])
PY

log "mock logind test passed; calls are in ${recordFile}"
