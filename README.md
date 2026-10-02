# ArmorPaint Free Library

A plugin for ArmorPaint that turns the **Plugins** tab into a browser for free
PBR materials, 3D models and HDRIs. Search, preview and download assets
straight into your library - each download is imported into the open project
immediately.

## Sources

| # | Source              | Types                 | API key   | License                          |
|---|---------------------|-----------------------|-----------|----------------------------------|
| 0 | [Poly Haven](https://polyhaven.com)   | Materials, Models, HDRIs | no    | CC0                              |
| 1 | [ambientCG](https://ambientcg.com)    | Materials             | no        | CC0                              |
| 2 | [poly.pizza](https://poly.pizza)      | Models                | free key  | per item (CC0 / PD / CC-BY)      |
| 3 | [Sketchfab](https://sketchfab.com)    | Models                | token     | CC0 / CC-BY / CC-BY-SA (filter)  |
| 4 | [Blendkit](https://www.blendkit.com)  | Models, HDRIs         | no        | free, check the item             |

- every download gets a `credits.txt` (author - license - source link) written
  next to the asset; Sketchfab items also get the full `license.txt`
- thumbnails are cached locally so browsing stays fast
- downloads land in your library folder (`LIB_ROOT`) and import right away

## Requirements

A current ArmorPaint build. The plugin uses these engine helpers:
`json_get`, `zip_extract`, `script_file_download` with an HTTP-headers
argument, and the long-URL fix in `iron_file_download_h` (Sketchfab serves
presigned download URLs that exceed 512 bytes).

## Install

1. Copy `free_library.c` into ArmorPaint's `data/plugins/` folder.
2. Start ArmorPaint, open **Preferences -> Plugins** and enable
   `free_library.c`.
3. Restart ArmorPaint. The **Plugins** tab now shows the Free Library panel.

## Configuration (`.env`)

Everything works out of the box with generic defaults. To use poly.pizza /
Sketchfab and to choose where files go, drop a `.env` file in either
`data/` (next to the `plugins` folder) or next to the executable:

```env
LIB_ROOT=./FreeLibrary
THUMB_DIR=./.freelib_thumbs
POLY_PIZZA_KEY=
SKETCHFAB_TOKEN=
```

See [.env.example](.env.example). Values are read once at startup - restart
after editing.

Free keys:

- **POLY_PIZZA_KEY** - register at <https://poly.pizza/settings/api>
- **SKETCHFAB_TOKEN** - <https://sketchfab.com/settings/password> -> API tokens

Poly Haven, ambientCG and Blendkit need no key at all. The `.env` file is
gitignored - never commit your keys.

## Selftest

The plugin contains a headless regression test used during development.
Set a mode in `free_library.c`:

```c
int selftest = 0; // 1=PH material  2=acg material  3=PH HDRI   4=PH model
                  // 5=poly.pizza   6=Sketchfab     7=Blendkit   8=Blendkit HDRI
```

then run it with the driver from [tools/selftest_driver.c](tools/selftest_driver.c):

```sh
rm -f /tmp/opencode/fl_done
./ArmorPaint --background --script tools/selftest_driver.c
```

Searches run headlessly, the first result is downloaded and imported, and the
driver exits when the marker file appears (`DRIVER: marker found`) or after
~120 seconds. Set `selftest` back to `0` for normal use.

## License

MIT - see [LICENSE](LICENSE). Assets keep their own licenses; that is why
`credits.txt` is written next to every download.
