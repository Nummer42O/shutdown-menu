# Tests

This directory contains the unit and integration tests for `shutdown-menu`.
They are intentionally plain shell scripts so they can be run from a terminal,
CTest, or VS Code without extra test frameworks.

## Requirements

For the unit tests:

- CMake >= 3.16 and a C compiler
- GLib tools (`glib-compile-schemas`, usually in `libglib2.0-bin`)
- `gdbus` (also in `libglib2.0-bin`) for the mock-logind test
- Python 3 with `gi` (`python3-gi`) for the keybind and mock-logind tests
- `dbus-run-session` (part of `dbus`) for the private mock-logind bus
- `argcomplete` is optional; the keybind script degrades gracefully without it

For the integration smoke tests:

- `Xephyr` (part of `xserver-xephyr`)
- `gnome-shell`/mutter for the nested Wayland test
- ImageMagick (`import`, `compare`) for screenshots
- `dbus-run-session` (part of `dbus`)
- A working X11 display to nest Xephyr in, or `Xvfb`

The scripts exit with status `77` when their prerequisites are unavailable.
CTest treats this as a skipped test (`SKIP_RETURN_CODE 77`), not a failure.

## Running

Configure the project with tests enabled (this is the default when
`BUILD_TESTING` is `ON`):

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
```

Then run all tests:

```bash
ctest --test-dir build --output-on-failure
```

Useful subsets:

```bash
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -R unit.keybind -V
```

Artifacts (screenshots, app logs, and mock-logind calls) are written to
`build/test-artifacts/` by default. Override this with the
`SHUTDOWN_MENU_TEST_ARTIFACTS` environment variable.

## Tests

### `unit.schema`

Runs `glib-compile-schemas --strict --dry-run` against the schema generated in
`build/schemas/` and checks that the dconf path ends with `/`. This catches the
class of bug where the CMake-generated application path is not a valid dconf
path.

### `unit.keybind`

Uses a temporary keyfile GSettings backend (`GSETTINGS_BACKEND=keyfile` plus a
temporary `XDG_CONFIG_HOME`), so it never touches the user's real dconf
database. It verifies:

- `enable` creates one custom-keybinding entry;
- running `enable` twice is idempotent;
- the `e`/`d` aliases work;
- `disable` clears the binding;
- `disable --purge` removes the entry.

### `unit.cli`

Runs the built binary with `--help` without a display and checks that the
public options (`--restart`, `--grace-period`) are advertised.

### `unit.mock-logind`

Exercises `helpers/mock-logind.py` through `helpers/with-mock-logind.sh`.
The helper starts a private D-Bus bus, sets `DBUS_SYSTEM_BUS_ADDRESS` to that
bus, and runs the command with a mock `org.freedesktop.login1` service. The
mock implements `PowerOff`, `Reboot`, `CanPowerOff`, and `CanReboot`, and
records every power/reboot request as JSON Lines.

This test calls `PowerOff(false)` and `Reboot(true)` with `gdbus` and verifies
the recorded methods and interactive flags. It does not start the GUI or touch
the real system bus.

### Mock logind helper

`helpers/with-mock-logind.sh` is intended for later action-execution tests:

```bash
tests/helpers/with-mock-logind.sh \
  --record build/test-artifacts/mock-logind/calls.jsonl \
  -- <command> [arguments...]
```

The command can use `DBUS_SYSTEM_BUS_ADDRESS` directly, or ordinary GDBus
`G_BUS_TYPE_SYSTEM` calls. Useful options:

- `--python PATH` selects the Python interpreter with `python3-gi`.
- `--can-result yes|no|na|challenge` changes what `CanPowerOff`/`CanReboot`
  return.
- `--exit-after N` stops the mock after N power/reboot calls.

The record file contains one object per call, for example:

```json
{"interactive": false, "method": "PowerOff", "time": 1795282911.869683}
```

A future GUI test can launch `shutdown-menu` inside the helper, select a button
or let the timeout expire, and then assert the recorded method. Cancel should
leave the record file empty. The helper always creates a private bus; it never
replaces the real logind service.

### `integration.wayland-smoke`

Starts a private nested GNOME Shell Wayland compositor (under Xephyr), launches
the application with `GDK_BACKEND=wayland` and
`G_DEBUG=fatal-criticals,fatal-warnings`, and checks that the screen changes
(the fullscreen dim overlay/dialog appeared). The app log and before/after
screenshots are kept in the artifact directory.

This test does **not** assert the final dialog position or multi-monitor
behavior yet; those are Phase 2 items. It is meant to catch X11-only code
paths, GTK criticals, and startup regressions on Wayland.

### `integration.x11-smoke`

Runs the application against a fresh Xephyr X11 display and verifies that it
maps a window/dialog. Xephyr alone has no window manager, so this test is
skipped unless `SHUTDOWN_MENU_X11_WM` points to a window manager binary (for
example `openbox` or an absolute path to it). Set it in the environment before
running CTest, or use the `Test: X11 smoke` VS Code task with the variable
exported.

### XTest helper

`helpers/xtest-click.c` is compiled to
`build/tests/shutdown-menu-test-click` when the XTest development files are
available. It sends a click to an X11 display:

```bash
build/tests/shutdown-menu-test-click :99 100 500
```

This is useful for manual focus/click-outside testing (including through the
nested Xephyr display used by the Wayland smoke test).

## Known limitations

- The Wayland smoke test needs a nested GNOME Shell; it is skipped when the
  required binaries are missing.
- Multi-monitor, primary-monitor selection, light/dark theme and click-outside
  behavior need dedicated tests in later phases.
- `G_DEBUG=fatal-warnings` is enabled for the integration tests. If GNOME or
  GTK itself emits an unrelated warning in a particular environment, inspect
  the artifact log before disabling the flag.
- The integration tests set `GTK_A11Y=test`, which uses GTK's test accessibility
  backend and does not require a live AT-SPI bus. `NO_AT_BRIDGE=1` is a GTK3
  setting and is ignored by GTK4.

## VS Code

The repository provides these tasks (Run Task / Terminal > Run Task):

- `Test: unit`
- `Test: all`
- `Test: schema`
- `Test: keybind`
- `Test: Wayland smoke`
- `Test: X11 smoke`

The existing `Build` task is a dependency of the test tasks.
