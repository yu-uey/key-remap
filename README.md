# key-remap
A simple keyboard remapping program for home-row-mods, written with C and libevdev.

## Building
Requires `libevdev-devel` (may be named differently depending on distribution). The Makefile adds the libevdev flags via `pkg-config`.

## Usage
Pass in the path to the keyboard device to remap (grab). Often contains "keyboard" in its name.
```
./key-remap /dev/input/by-path/your-keyboard
```
You may need to add yourself to the `input` group. Check permissions on the device:
```
stat -c '%U:%G' $(readlink -f /dev/input/by-path/your-keyboard)
```

## Notes
- There is no configuration file. To change the mappings, edit the source, and modify the `modtaps` array and `tapterm` value.
- Tap-hold is based on the timing of key releases. If any key is both pressed and released while a tap-hold is pressed, it retroactively applies a hold. This enables instant modifier activation without significantly affecting the regular typing experience. A tap-hold key held beyond its tap term activates its hold key, which allows modifiers to be used easily with mouse inputs.
- More full-featured programs exist, e.g. [KMonad](https://github.com/kmonad/kmonad), [Kanata](https://github.com/jtroo/kanata) and [keyd](https://github.com/rvaiya/keyd). This program is intentionally bare-bones because I wanted to quickly try specific handling behaviours tailored to my own typing habits (like branching based on which finger I use to type each key, which turned out not so useful).
- Originally started writing in Rust (see `rust` branch). Switched to C because it made `rpmbuild` packaging (from source) much easier, removing the cargo dependencies.
