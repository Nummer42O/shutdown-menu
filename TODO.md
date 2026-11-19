# shutdown-menu implementation plan

Status: `dev` branch, based on `main` after merging `feature/timing`.
The test scaffold and the minimal build/guard fixes are already in place.

## Decisions / clarifications

- Grace period = the duration of the countdown timer shown in the dialog.
  When it reaches zero, the preselected action is performed. It is not a
  separate post-selection delay.
- All monitors must be dimmed; the dialog appears on one preferred monitor.
- The dim overlay must cover the dock and top bar (fullscreen, not merely the
  work area).
- Preferred monitor detection:
  - X11: RandR primary output.
  - Wayland/GNOME: Mutter `DisplayConfig` over D-Bus.
  - No special fallback is required; if detection is unavailable, current
    behavior is acceptable.
- Application identity:
  - Application/schema ID: `io.github.Nummer42O.ShutdownMenu`.
  - Executable/CMake target: `shutdown-menu`.
  - GSettings path: `/io/github/Nummer42O/ShutdownMenu/`.
- The custom dialog window is the preferred implementation. `GtkMessageDialog`
  was a stopgap and should be replaced.
- `GDK_WINDOWING_X11` is a compile-time guard for the X11 backend/header.
  `GDK_IS_X11_SURFACE()` is the runtime guard for the actual surface. Both are
  needed:
  `#ifdef GDK_WINDOWING_X11` around the include/call, and
  `if (GDK_IS_X11_SURFACE(windowSurface))` before the call.
- Tests live in `tests/`; CTest, VS Code tasks and the XTest helper are set up.

## Phase 0 - baseline, test infrastructure, safety

- [x] Merge `feature/timing` into `main`; create `dev`.
- [x] Add `tests/` with:
  - `unit.schema` (strict schema validation)
  - `unit.keybind` (temporary keyfile backend, idempotence/aliases/purge)
  - `unit.cli` (`--help`, public options)
  - `integration.wayland-smoke` (nested GNOME Shell under Xephyr)
  - `integration.x11-smoke` (fresh Xephyr X11 display)
  - XTest click helper for manual focus/click testing
- [x] Register tests with CTest (`BUILD_TESTING`, labels `unit`/`integration`,
  skip status 77 when optional dependencies are absent).
- [x] Add VS Code tasks for unit, all, schema, keybind, Wayland smoke and X11
  smoke; the existing `Build` task is a dependency.
- [x] Document the test prerequisites, behavior, artifacts and limitations in
  `tests/README.md`.
- [x] Guard the X11-only skip-taskbar call with `GDK_WINDOWING_X11` and
  `GDK_IS_X11_SURFACE()`; drop the unconditional `gtk4-x11` build requirement.
- [x] Fix the generated GSettings path (trailing slash) and configure/build the
  schema from CMake.
- [x] Fix keybind helper idempotence, aliases, duplicate paths, regex
  robustness, and optional `argcomplete`.
- [ ] Add `--dry-run`/fake-systemctl test support so final actions can be
  verified without invoking systemd/polkit.

Acceptance: `cmake --build build` succeeds and `ctest --test-dir build -L unit`
passes. Integration tests either pass or skip with status 77.

## Phase 1 - build, identity, packaging, settings foundation

- [x] Rename the CMake target/output to `shutdown-menu`; keybind default
  command/install path updated.
- [x] Change the application ID to `io.github.Nummer42O.ShutdownMenu`; keep
  `SHUTDOWN_MENU_APPLICATION_ID` as a validated cache option.
- [ ] Existing build trees keep the old cached application ID; reconfigure
  with `-DSHUTDOWN_MENU_APPLICATION_ID=io.github.Nummer42O.ShutdownMenu`. Old
  dconf/keybind entries require manual migration.
- [ ] Rename compile definitions/macros to `SHUTDOWN_MENU_*`:
  `SHUTDOWN_MENU_APPLICATION_ID`, `SHUTDOWN_MENU_DRY_RUN`,
  `SHUTDOWN_MENU_STYLESHEET_DIR`, `SHUTDOWN_MENU_SYSTEMCTL`.
- [ ] Move internal response constants and static declarations out of headers;
  remove or rebuild the dead `dialog.c`/`dialog.h`.
- [ ] Fix stylesheet/resource discovery for installed binaries:
  use a GResource (preferred) or a runtime search through the system data
  directories; do not embed a build-tree path.
- [ ] Install the keybind script with `install(PROGRAMS ...)` from the build
  tree; keep the source-tree copy only as a developer convenience or remove it.
- [ ] Remove `CONFIGURATIONS Release ...` filters so Debug builds can be
  installed.
- [ ] Install the schema to `${DATADIR}/glib-2.0/schemas`; run
  `glib-compile-schemas --strict` at install time (respect `DESTDIR`).
- [ ] Add a matching `.desktop` file and application name:
  `io.github.Nummer42O.ShutdownMenu.desktop`, `Icon=shutdown-menu`.
- [ ] Add GSettings keys:
  `countdown-seconds`, `countdown-update-interval`, `theme` (auto/light/dark),
  `dialog-monitor` (connector or auto), and optionally
  `dim-all-monitors`.
- [ ] Load settings defensively: use defaults when the schema is unavailable;
  keep CLI overrides (`--restart`, `--grace-period`) authoritative.
- [ ] Document `GSETTINGS_SCHEMA_DIR` for development and install.

Acceptance: installable Debug/Release builds; no build-tree paths in the
installed binary; schema validates and installs; the app starts with or without

## Phase 2 - window architecture (Wayland + multi-monitor)

- [ ] Replace the single maximized background window + `GtkMessageDialog` with
  one fullscreen overlay window per `GdkMonitor`.
- [ ] Each overlay: undecorated, fullscreen on its monitor, opaque black,
  `gtk_widget_set_opacity(0.5)`, click-to-cancel. This covers the dock and top
  bar as requested (verified with nested GNOME).
- [ ] Put a custom dialog card in the preferred monitor overlay only:
  title, message, countdown, Cancel/Restart/Power Off buttons; centered with
  `GtkCenterBox`/`GtkOverlay`.
- [ ] Preferred monitor:
  - X11: RandR primary output.
  - Wayland/GNOME: Mutter `DisplayConfig.GetCurrentState`; read the
    `primary` logical monitor and map it to a GdkMonitor connector.
  - If unavailable: keep current behavior (no hard failure).
- [ ] Persist a manual override via GSettings/CLI (`dialog-monitor` connector),
  matching `gdk_monitor_get_connector()`.
- [ ] Handle `GListModel::items-changed`, monitor invalidation/removal and
  hotplug: rebuild overlays; if the preferred monitor disappears, keep the
  current behavior rather than crashing.
- [ ] Present overlays on secondary monitors first, then the preferred
  monitor/card last; focus the default button after mapping.
- [ ] Cancellation: Escape shortcut, click on any overlay, and close-request.
  Defer teardown to an idle callback; destroy dialog/windows with their parent;
  no synchronous `gtk_window_destroy()` inside event/response callbacks.
- [ ] X11: keep the guarded skip-taskbar hint. Wayland: no skip-taskbar hint
  exists; document the limitation.
- [ ] Handle `GApplication` re-activation without creating duplicate timers or
  dialogs.

Acceptance: under X11 and Wayland, all monitors are dimmed, the card is
centered on the selected monitor, Escape/click cancel, no Gtk criticals with
`G_DEBUG=fatal-criticals,fatal-warnings`, and monitor hotplug does not crash.

## Phase 3 - countdown, grace period, settings, focus

- [ ] Implement `--grace-period` as the dialog countdown duration; the
  preselected action (power off/restart) is applied when it reaches zero.
- [ ] Use a monotonic deadline (`g_get_monotonic_time`) and compute remaining
  seconds on each tick instead of repeatedly subtracting from an unsigned
  counter; clamp interval/remaining values.
- [ ] Store and remove timeout source IDs on response/dispose; do not leave
  timers referencing freed state.
- [ ] Update the displayed countdown immediately when settings change and
  restart the timer from the new duration.
- [ ] Integrate GSettings:
  `countdown-seconds`, `countdown-update-interval`, `theme`, `dialog-monitor`.
- [ ] CLI precedence: `--restart` and `--grace-period` override settings for
  this invocation.
- [ ] Make `GApplication` activation idempotent: a second launch while the
  menu is open should present/reset the existing menu, not create a second
  dialog or timer set.
- [ ] Defer close/quit until the response has been recorded; ensure
  `finalAction` is set exactly once.

Acceptance: countdown and timer display match; timeout performs the default
action; explicit buttons perform the selected action; Cancel always yields no
action; a second launch cannot create duplicate timers.

## Phase 4 - light mode / theme

- [ ] Replace the hardcoded `#shutdown-dialog` dark background with
  theme-aware styling.
- [ ] Add `theme=auto|light|dark` to GSettings; `auto` follows
  `GtkSettings:gtk-application-prefer-dark-theme` and the system color scheme.
- [ ] Apply the theme to the custom card and buttons; keep the dim overlay black
  (opacity-based) in both modes, as verified.
- [ ] Listen for theme changes and update the live dialog.
- [ ] Add screenshot tests for `GTK_THEME=Adwaita:light` and `Adwaita:dark`.

Acceptance: light and dark modes are usable and follow the system in `auto`;
the overlay still dims correctly on Wayland and X11.

## Phase 5 - action execution, keybind, Wayland session integration

- [ ] Make the `systemctl` path configurable (`SHUTDOWN_MENU_SYSTEMCTL`) and
  add a runtime `--dry-run` or fake-command test path.
- [ ] Verify final action selection separately from execution:
  cancel -> no exec; timeout -> default; buttons -> selected option.
- [ ] Review `setuid`/`setgid`/`execve` behavior and environment sanitization;
  preserve only the variables needed by systemd/polkit.
- [ ] On Wayland, the UI runs as the user and systemctl triggers the polkit
  agent after the overlay closes; do not run the GUI as root.
- [ ] Keybind: keep the corrected idempotence/alias behavior; verify
  custom-keybindings are per-user and work under Wayland session services.
- [ ] Add the `.desktop` file and icon so the app has a stable identity; accept
  that Wayland has no skip-taskbar equivalent.

Acceptance: fake-systemctl tests pass; a real Wayland session prompts through
polkit; keybind enable/disable is idempotent and does not leave duplicates.


## Phase 6 - documentation, CI, release

- [ ] Update README:
  build dependencies (`libglib2.0-bin`, `python3-gi`, optional
  `argcomplete`), Wayland/X11 behavior, multi-monitor/primary settings,
  keybind usage, test commands, fake-systemctl/dry-run testing.
- [ ] Add a CI job for Debug/Release builds, `-Wall -Wextra -Werror`,
  schema validation, keybind idempotence, CLI help and fake-systemctl actions.
- [ ] Optional CI integration job: nested GNOME Wayland smoke test; tolerate
  missing compositor with skipped tests.
- [ ] Bump the version and document migration (app ID, dconf path, executable
  name, keybind command).
- [ ] Verify `uninstall` removes the schema/keybind/desktop/icon additions.

Acceptance: fresh clone -> configure -> build -> `ctest` works; README matches
the installed behavior and the new identity.

## Test matrix

| Area | X11 | Wayland | Settings/CLI |
|---|---|---|---|
| Startup | no warnings/criticals | no X11 symbols, no warnings/criticals | schema optional |
| Schema | validates with `--strict --dry-run` | same | installed/loadable |
| Overlay | every monitor fullscreen dimmed | every monitor fullscreen dimmed | `dim-all-monitors` |
| Card placement | centered on preferred monitor | centered on preferred monitor | `dialog-monitor` |
| Primary detection | RandR primary | Mutter DisplayConfig primary | connector override |
| Countdown | correct timeout/display | correct timeout/display | `countdown-seconds`, `countdown-update-interval` |
| Default action | matches `--restart`/power off | same | CLI precedence |
| Buttons | Cancel/Restart/Power Off correct | same | — |
| Escape | cancels | cancels | — |
| Click outside | cancels | cancels | — |
| Focus | default button focused; arrows work | default button focused; arrows work | — |
| Teardown | no Gtk criticals | no Gtk criticals | timers removed |
| Multi-monitor hotplug | rebuilds overlays | rebuilds overlays | preferred monitor fallback |
| Theme | light/dark/auto | light/dark/auto | `theme` |
| Keybind | enable/disable/purge idempotent | same (GSettings/gsd) | keyfile backend test |
| Action execution | cancel = no exec; timeout/buttons correct | polkit prompt after overlay closes | fake systemctl / dry-run |

## Verification commands

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
```

Test artifacts: `build/test-artifacts/` (Wayland/X11 screenshots and logs).

## Notes / gotchas

- `gtk_window_maximize()` only covers the work area on Wayland; use
  `gtk_window_fullscreen()`/`gtk_window_fullscreen_on_monitor()` for the dim
  overlay.
- CSS `background: rgba(0,0,0,0.5)` did not dim on Wayland in testing; use an
  opaque black window plus `gtk_widget_set_opacity()`.
- A modal dialog centers correctly, but blocks click-outside. If a separate
  dialog is ever used, clear modality after mapping or use the custom card.
- `gdk_display_get_primary_monitor()`/`gdk_monitor_is_primary()` do not exist
  in GTK4 4.6; use RandR/Mutter plus `gdk_monitor_get_connector()`.
- Synchronous `gtk_window_destroy()` from focus/response callbacks causes
  Gtk criticals; defer with `g_idle_add()` and set `destroy-with-parent`.
- `GDK_WINDOWING_X11` is a compile-time guard; `GDK_IS_X11_SURFACE()` is the
  runtime guard. Use both around X11-only calls.
