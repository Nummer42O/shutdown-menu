# Shutdown Menu

A simple shutdown menu in the style of Gnome 3.36 because with Gnome 42.9 it got split in two.

## Installation

Install by cloning this repository and building it using cmake.
Building requires:
- GTK 4 C (library and headers)
- Glib 2 C (library and headers)
- `glib-compile-schemas` (usually in `libglib2.0-bin`)
- CMake >= 3.16

```bash
git clone --branch main git@github.com:Nummer42O/shutdown-menu.git
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release -S ./ -B ./build
cmake --build ./build
sudo cmake --build ./build --target install --config Release
```

CMake will also configure a `shutdown-menu-keybind` script in the build directory.
The script may be used to install a custom keybind called "Shutdown Menu" into the Gnome settings daemon (`org.gnome.settings-daemon.plugins.media-keys`).
See `shutdown-menu-keybind --help` for more information.
The script also supports python3 argcomplete.

Alternatively it can be executed as CMake targets as follows:

```bash
# Try enabling the keybind, will fail if a conflicting keybind exists.
cmake --build ./build --target enable-keybinds
# Always installs the keybind, will deactivate conflicting keybinds by unsetting their shortcut.
cmake --build ./build --target force-enable-keybinds
```

## Usage

The intended way of running the script is via the keyboard shortcut.
Alternatively it can be run from the commandline or via the desktop file.

The executable provides two runtime toggles:
1. `--dry-run`: Mainly used for testing. It allows the full execution of the program without the final poweroff/reboot.
2. `--restart`: Selected "restart" instead of "poweroff" as the default action on timeout.
For more info and glib options, see `shutdown-menu --help`.

> [!WARNING]
> Should you decide to use a non-default `CMAKE_INSTALL_PREFIX` you must specify `GSETTINGS_SCHEMA_DIR` to point to the GSettings schema like this: `GSETTINGS_SCHEMA_DIR="${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas"`.
> Similar rules apply if you decide against an installation, then the schemas are located under `<workspace>/build/schemas`.

## Uninstallation

Be sure to keep the original repository if you plan to take advantage of the uninstall target.
To use it, execute the following from within the repository folder:
```bash
sudo cmake --build ./build --target uninstall
```

`shutdown-menu-keybind disable` can be used to disable (or remove) the previously installed keybind.
Similarly to the installation process, there are CMake targets for this too:

```bash
# Disable the installed keybind by unsetting the shortcut.
cmake --build ./build --target disable-keybinds
# Completly remove the keybind entry from "org.gnome.settings-daemon.plugins.media-keys".
cmake --build ./build --target purge-keybinds
```

## Testing

The repository includes unit tests (schema validation, CLI, keybind helper) and
optional integration smoke tests for X11 and a nested GNOME Wayland session.

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

See [`tests/README.md`](tests/README.md) for prerequisites, the full test list,
artifacts and known limitations. VS Code tasks named `Test: ...` are also
provided.
