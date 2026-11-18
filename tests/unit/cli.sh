#!/usr/bin/env bash
# Check the public command line surface without starting a GUI.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

app="${1:?'missing application binary'}"
need_file "$app"

output="$(env -u DISPLAY -u WAYLAND_DISPLAY "$app" --help 2>&1)" || die "--help failed"
[[ "$output" == *"--restart"* ]] || die "--help does not advertise --restart"
[[ "$output" == *"--grace-period"* ]] || die "--help does not advertise --grace-period"

log "CLI help test passed"
