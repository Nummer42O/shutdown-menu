# Shutdown Menu

A simple shutdown menu in the style of Gnome 3.36 because with Gnome 42.9 it got split in two.

# Installation

Install by cloning this repository and building it using cmake.
Building requires:
- GTK 4 C (library and headers)
- Glib 2 C (library and headers)
- CMake >= 3.16

```bash
git clone --branch main git@github.com:Nummer42O/shutdown-menu.git
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release -S./ -B./build
cmake --build ./build
sudo cmake --install ./build --config Release
```

CMake will also configure the `shutdown-menu-keybind` script which can be used to install a custom keybind called "Shutdown Menu".
See `shutdown-menu-keybind enable --help` for more information. The script also supports python3 argcomplete.

# Uninstallation

Be sure to keep the original repository if you plan to take advantage of the uninstall target.
To use it, execute the following from within the repository folder:
```bash
sudo cmake --build ./build --target uninstall
```

`shutdown-menu-keybind disable` can be used to disable the previously installed command.
