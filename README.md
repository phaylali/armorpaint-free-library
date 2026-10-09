# ArmorPaint Free Library

Browse and download **free** PBR materials, 3D models and HDRIs from five
public sources, straight from ArmorPaint's **Plugins** tab. Every download is
saved into your library folder and imported into the open project
immediately.

## Sources

| Source | What it offers | License | API key |
|---|---|---|---|
| [Poly Haven](https://polyhaven.com) | Materials, models, HDRIs | CC0 | no |
| [ambientCG](https://ambientcg.com) | Materials | CC0 | no |
| [poly.pizza](https://poly.pizza) | Models (game-ready) | CC0 / PD / CC-BY per item | free key |
| [Sketchfab](https://sketchfab.com) | Models | CC0 / CC-BY / CC-BY-SA (filterable) | free token |
| [Blendkit](https://www.blendkit.com) | Models, HDRIs | free, check the item | no |

Three of the five sources work with **no key at all**. The two keys are free
and self-service — see below.

## Install

1. Copy `free_library.c` into ArmorPaint's `data/plugins/` folder.
2. Start ArmorPaint, open the **Plugins** tab, click **Preferences** and
   enable `free_library.c`.
3. Restart ArmorPaint. The Plugins tab now shows the **Free Library** panel.

## Setting up your API keys (no file editing)

1. Open the **Plugins** tab. The **Settings** section opens by itself the
   first time (while a key is missing); afterwards the **Settings** button
   at the bottom of the panel opens it.
2. Get your free keys (one click each in the panel):
   - **poly.pizza key** — <https://poly.pizza/settings/api> → *Generate*
   - **Sketchfab token** — <https://sketchfab.com/settings/password> →
     *API tokens* → *Generate Token*
3. Paste each key into its field and press **Save**.

That's it — the keys are stored in `data/.env` for you and take effect on
the next start. Poly Haven, ambientCG and Blendkit keep working without any
key, so you can ignore this step entirely if you don't need poly.pizza or
Sketchfab.

<details>
<summary>Manual <code>.env</code> setup (alternative)</summary>

Create `data/.env` (or `.env` next to the executable):

```env
LIB_ROOT=./FreeLibrary
THUMB_DIR=./.freelib_thumbs
POLY_PIZZA_KEY=
SKETCHFAB_TOKEN=
```

See [.env.example](.env.example). Values are read once at startup — restart
after editing. The GUI Settings panel reads and writes the same file.
</details>

## How to use

1. Pick a **Source** (Poly Haven, ambientCG, …), a **Type**
   (Materials / Models / HDRIs) and a **Quality**.
2. Type a search term and press **Search** — thumbnails load as you scroll.
3. Press **Get** on a result. The asset downloads into your library folder
   and is imported into the project right away.

Every download is written next to a `credits.txt`
(`author - license - source link`), so attribution is always one file away.
Sketchfab downloads also include the full `license.txt`.

## Where files go

Defaults: `./FreeLibrary` for assets, `./.freelib_thumbs` for the thumbnail
cache (both relative to where ArmorPaint starts). Change either in `.env` —
see *Manual setup* above.

## Requirements

A current ArmorPaint build that includes the Free Library engine helpers:
`json_get`, `zip_extract`, `script_file_download` with an HTTP-headers
argument, the `path_data` binding, and the long-URL fix in
`iron_file_download_h` (Sketchfab serves presigned download URLs longer than
512 bytes).

## Selftest (development)

The plugin has a built-in headless regression test. Set a mode in
`free_library.c`:

```c
int selftest = 0; // 1=PH material  2=acg material  3=PH HDRI   4=PH model
                  // 5=poly.pizza   6=Sketchfab     7=Blendkit   8=Blendkit HDRI
```

then run it with the driver from [tools/selftest_driver.c](tools/selftest_driver.c):

```sh
rm -f /tmp/opencode/fl_done
./ArmorPaint --background --script tools/selftest_driver.c
```

The first result is downloaded and imported; the run ends with
`DRIVER: marker found`. Set `selftest` back to `0` for normal use.

## License

MIT — see [LICENSE](LICENSE). Downloaded assets keep their own licenses;
that is why `credits.txt` is written next to every download.

## Connect With Us

- [Discord](https://discord.omniversify.com) — Join our community
- [X/Twitter](https://twitter.com/omniversify) — Follow updates
- [GitHub](https://github.com/phaylali) — Explore our work
- [RSS Feed](/rss.xml) — Subscribe to updates

## Support Us

<p align="center">
  <a href="https://ko-fi.com/omniversify">
    <img src="https://raw.githubusercontent.com/phaylali/Omniversify/main/public/images/kofi_logo.svg" width="200" alt="Ko-Fi" />
  </a>
</p>

<p align="center">
  <strong>Keep us going</strong>
</p>

---

Licensed under [MIT](LICENSE).

_Made by Moroccans, for the Omniverse_

[![ReadMeSupportPalestine](https://raw.githubusercontent.com/Safouene1/support-palestine-banner/master/banner-project.svg)](https://donate.unrwa.org/-landing-page/en_EN)
