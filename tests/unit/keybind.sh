#!/usr/bin/env bash
# Test the keybind helper against a throwaway keyfile GSettings backend.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

python="${1:?'missing python interpreter'}"
keybindScript="${2:?'missing generated keybind script'}"
commandPath="${3:?'missing command path'}"

need_file "$keybindScript"
need_cmd "$python"
need_cmd gsettings

"$python" - <<'PY' || skip "python3-gi and the GNOME media-keys schema are required"
import sys
try:
    from gi.repository import Gio
except ImportError:
    sys.exit(1)
source = Gio.SettingsSchemaSource.get_default()
if source is None or source.lookup("org.gnome.settings-daemon.plugins.media-keys", True) is None:
    sys.exit(1)
PY

tmpDir="$(make_temp_dir keybind-test)"
function cleanup {
  kill_bg
  rm -rf "$tmpDir"
}
trap cleanup EXIT

export XDG_CONFIG_HOME="${tmpDir}/config"
export GSETTINGS_BACKEND=keyfile
mkdir -p "$XDG_CONFIG_HOME"
unset GSETTINGS_SCHEMA_DIR || true

customKeybindingsSchema="org.gnome.settings-daemon.plugins.media-keys"
customKeybindSchema="${customKeybindingsSchema}.custom-keybinding"
custom0="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/"

function run_keybind {
  "$python" "$keybindScript" --binding '<Alt>F3' --command "$commandPath" "$@"
}

function custom_path_count {
  gsettings get "$customKeybindingsSchema" custom-keybindings \
    | grep -o '/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom[0-9]*/' \
    | wc -l | tr -d ' ' || true
}

function assert_count {
  local expected="$1"
  local actual
  actual="$(custom_path_count)"
  [[ "$actual" == "$expected" ]] || die "expected ${expected} custom keybinding(s), got ${actual}"
}

function get_binding {
  gsettings get "${customKeybindSchema}:${custom0}" binding
}

log "enable"
run_keybind enable
assert_count 1
[[ "$(get_binding)" == "'<Alt>F3'" ]] || die "unexpected binding: $(get_binding)"

log "enable must be idempotent"
run_keybind enable
assert_count 1

log "enable via the 'e' alias"
run_keybind e
assert_count 1

log "disable"
run_keybind disable
assert_count 1
[[ "$(get_binding)" == "''" ]] || die "disable did not clear the binding: $(get_binding)"

log "re-enable with override and purge"
run_keybind enable --problem-handler override
assert_count 1
run_keybind d --purge
assert_count 0

log "keybind tests passed"
