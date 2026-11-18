#!/usr/bin/env bash
# Shared helpers for the shutdown-menu test scripts.
set -euo pipefail

function log {
  printf '==> %s\n' "$*" >&2
}

function die {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

function skip {
  printf 'SKIP: %s\n' "$*" >&2
  exit 77
}

function need_cmd {
  local cmd="$1"
  command -v "$cmd" >/dev/null 2>&1 || skip "required command not found: $cmd"
}

function need_file {
  local file="$1"
  [[ -e "$file" ]] || die "missing file: $file"
}

function make_temp_dir {
  local prefix="${1:-shutdown-menu-test}"
  mktemp -d "${TMPDIR:-/tmp}/${prefix}.XXXXXX"
}

# Run a command, logging it first.
function run {
  log "$*"
  "$@"
}

# Wait until a predicate exits successfully or the timeout expires.
# Usage: wait_until <seconds> <description> <command...>
function wait_until {
  local timeout="$1"
  local description="$2"
  shift 2
  local i
  for ((i = 0; i < timeout; i++)); do
    if "$@" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

# Start a background process and remember its PID for cleanup.
# The PID is available as $lastBgPid after the call.
declare -a shutdownMenuTestPids=()
lastBgPid=""
function start_bg {
  "$@" &
  lastBgPid=$!
  shutdownMenuTestPids+=("$lastBgPid")
}

function kill_bg {
  local pid
  if (( ${#shutdownMenuTestPids[@]} == 0 )); then
    return 0
  fi
  for pid in "${shutdownMenuTestPids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -TERM "$pid" 2>/dev/null || true
    fi
  done
  for pid in "${shutdownMenuTestPids[@]}"; do
    local i
    for ((i = 0; i < 20; i++)); do
      kill -0 "$pid" 2>/dev/null || break
      sleep 0.1
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -KILL "$pid" 2>/dev/null || true
    fi
  done
  wait 2>/dev/null || true
  shutdownMenuTestPids=()
}
