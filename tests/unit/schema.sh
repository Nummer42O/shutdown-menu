#!/usr/bin/env bash
# Validate the generated GSettings schema.
set -euo pipefail

scriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${scriptDir}/../helpers/common.sh"

schemaDir="${1:?'missing schema directory'}"
compiler="${2:-glib-compile-schemas}"

need_file "${schemaDir}/shutdown-menu.gschema.xml"
need_cmd "$compiler"

schemaFile="${schemaDir}/shutdown-menu.gschema.xml"

# The dconf path must begin and end with a slash.
if ! grep -Eq 'path="/[^"]*/"' "$schemaFile"; then
  die "schema path does not end with '/': $(grep -o 'path="[^"]*"' "$schemaFile" || true)"
fi

log "validating ${schemaFile}"
"$compiler" --strict --dry-run "$schemaDir"
log "schema validation succeeded"
