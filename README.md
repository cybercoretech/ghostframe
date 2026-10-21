# 👻 GHOSTFRAME

### 🖼️ A Cybercore photo archive for your desktop

**Ghostframe** is a local-first, keyboard-friendly photo browser for Omarchy and Linux desktops. It connects to your own [PhotoPrism](https://www.photoprism.app/) library and gives it a distinct Cybercore interface: near-black surfaces, sharp borders, neon cyan/pink/lime accents, mono display type, and a distraction-free image viewer.

> ⚡ **Status:** desktop MVP. The client builds and has been verified against the local PhotoPrism API and thumbnail service. Android is deliberately a later project and will live in its own repository.

**[Project site](https://cybercoretech.github.io/ghostframe/)**

## ✨ What Ghostframe is

Ghostframe is a native Qt Quick desktop client, not a rebranded PhotoPrism web build. PhotoPrism remains the photo catalog, indexer, and AI engine; Ghostframe is the custom browsing experience. Your image files stay in the storage locations you choose, and Ghostframe does not copy or edit originals.

The current client foundation includes:

- 🌌 A dark Cybercore desktop layout with a collection rail, image grid, status strip, and full-screen viewer.
- 🔎 PhotoPrism API connection, bearer-token authentication, text search, paged photo results, and a nested browser for your original folders.
- 🧭 Keyboard-first: `Ctrl+K` search, `[` / `]` previous/next photo, `Esc` / `Q` step back a layer, plus editor keys (see *Keyboard shortcuts*).
- 🔐 PhotoPrism credentials are restored automatically on launch. Ghostframe saves the connection in an owner-only local file and never contacts the desktop keyring, so launching it cannot trigger a keyring unlock prompt.
- 🖼️ Capability-token thumbnails served directly by PhotoPrism, without sending account credentials in image URLs.
- 🎛️ **Image Lab**, a darkroom editor on an in-process ⚡ **libvips** engine:
  - 🌗 **Light:** exposure, contrast, highlights, shadows, whites, blacks, levels and point curves (RGB + per channel).
  - 🎨 **Colour:** white balance (temperature / tint), vibrance, saturation, and 8-band HSL.
  - 🔬 **Detail:** sharpening, luminance and colour noise reduction.
  - 📐 **Geometry:** rotate, flip, straighten, perspective, and crop with aspect presets.
  - 📊 **Tools:** a live histogram, clipping warnings, a before/after split, true 1:1 zoom and a clickable history.
  - ⚡ Sliders preview live (a small draft while dragging, full quality when you let go), rendered off the UI thread.
- ✨ **Cyber FX:**
  - Effects: neon bloom, chromatic aberration, glitch, pixel sort, halftone, scanlines, film grain, vignette.
  - **CYBERGRID gradient maps** built from the 81 Cybercore palettes (neon, sunset, acid, duotones…).
  - `.cube` **LUTs**.
  - **Text & watermark layers** in JetBrains Mono with neon glow.
- 🎭 **Looks:** 9 built-in Cybercore looks (Neon Noir, Neon Coffin, Blackbox Terminal, Synthwave Sunset, Cyberdyne Red, Ghost Signal, Halftone Zine, Chrome Drip, Faded Polaroid) plus your own. One click styles a photo and keeps its crop and text.
- 📤 **Export presets:** YouTube Shorts 9:16 (fill, or fit over a blurred background), 16:9 thumbnail, square, web. Formats are JPG, PNG, WebP and AVIF, with adjustable quality.
- ↩️ Undo / redo for every edit, with a labelled history list. A slider drag counts as one step.
- 📜 **Edit recipes:** every edit is a versioned JSON recipe, rendered identically for preview and export, saved with the export, and **remembered per photo**. Reopen a photo (or move to it with `[` / `]`) and your edit is still there.
- 🧬 **Cybermeta inside:** metadata goes through the [cybermeta](https://github.com/cybercoretech/cybermeta) CLI.
  - A **METADATA** panel shows camera, lens, exposure, capture time and **location with a map link**.
  - Exports are **private by default**: GPS is stripped, or choose KEEP ALL, ANONYMOUS (no camera identity) or STRIP ALL.
  - Title, caption and tags are **embedded in the exported file** (XMP, plus EXIF ImageDescription).
  - A `.xmp` sidecar sits next to each export, with the source identity and the full edit recipe.
- 📤 Safe exports to `~/Pictures/Ghostframe/Exports/`; the source image stays untouched.
- 🐧 Native Linux packaging groundwork for Omarchy/Arch.

## 🧱 Architecture

```text
┌──────────────────────────────┐       PhotoPrism REST API
│ Ghostframe desktop (Qt Quick)│ ───────────────────────────┐
│ Cybercore browse / search    │                            │
└──────────────────────────────┘                            ▼
                                                  ┌──────────────────────┐
                                                  │ PhotoPrism service   │
                                                  │ catalog · AI · index│
                                                  └──────────┬───────────┘
                                                             │
                                                             ▼
                                                   Your photo storage
```

Inside the app:

```text
qml/            Qt Quick UI: Main.qml (window shell) + one file per component,
                Theme.qml singleton (palette, fonts)
src/photoservice.*    PhotoPrism API client and photo/folder models
src/imageeditor.*     Editor state for QML: recipe, undo history, async
                      preview/export, Cybermeta XMP sidecar
src/imageengine.*     libvips pipeline: load, preview proxy, apply recipe,
                      to-QImage, full-resolution export (pure, thread-safe)
src/editrecipe.*      Versioned JSON edit recipe (one table drives JSON,
                      QML keys and clamping)
src/edithistory.*     Labelled undo/redo over recipe states
src/recipestore.*     Per-photo remembered edits (<AppData>/recipes/*.json)
src/fx.*              Cyber FX + text layers (whole-frame, seeded, scale-aware)
src/cubelut.*         .cube 3D LUT parser/applier and the LUT library
src/cybergrid.*       CYBERGRID palette catalog + gradient styles (QML: cybergrid)
src/looklibrary.*     Built-in and user looks
src/cybermetaclient.* Bridge to the cybermeta CLI: summary, EXIF privacy
                      (via a JPEG carrier), XMP packets and sidecars
assets/cybergrid/     81 palettes synced from cybercore (tools/sync-cybergrid.sh)
assets/looks/         Built-in looks (JSON recipe fragments)
src/previewprovider.h image://ghostframe-edit/{preview,before,histogram,zoom}
                      frames for QML (no temp files)
tests/                Qt Test suite (engine, recipe, history, store,
                      editor end to end)
```

🧪 **Preview pipeline.** Opening a photo builds two small in-memory copies with libvips' shrink-on-load: a 1600×1200 *proxy* and an 800 px *draft*. While a slider is dragged, the draft is rendered. When it's released, the proxy is. Renders run on a worker thread, and if they queue up only the newest runs. Each render also produces the histogram, and on request clipping warnings and a "before" frame. **1:1 zoom** decodes the original once, keeps it in memory, and renders only the visible region at full resolution. **Export** runs the same recipe on the full-resolution original, writes a hidden staging file, and renames it into place only after it succeeds.

⚙️ **Render order** (in `ImageEngine::apply`):

1. **Geometry:** rotate 90° → flip → straighten (auto-crops the corners) → perspective → crop.
2. **Light + tone:** white balance and exposure (in linear light), levels, blacks/whites, shadows/highlights, brightness/contrast and curves. These are all per-channel, so they become **one lookup table** applied directly to the 8- or 16-bit pixels, with 16-bit output.
3. **Colour:** vibrance and saturation as luminance blends (a single 3×3 matrix when vibrance is off), then HSL bands in LCh.
4. **Detail:** colour noise, edge-aware luminance noise, then unsharp-mask sharpening. Radii scale with the preview size, so preview and export match.
5. **Cyber FX:** gradient map → LUT → bloom → chromatic aberration → glitch → pixel sort → halftone → scanlines → grain → vignette. Positions are fractions of the frame and random effects use fixed seeds, so preview, 1:1 zoom and export match.
6. **Text layers**, last. Sizes and margins are fractions of the output height, so they hold at any export size.

1:1 zoom cuts its region *after* the whole pipeline. Effects depend on the full frame, and libvips computes only the pixels the region needs (plus filter margins).

The interface talks to PhotoPrism's documented API. PhotoPrism keeps responsibility for indexing and AI; Ghostframe focuses on browsing. This keeps the app independent of PhotoPrism's internal database and makes it easier to track upstream changes.

## 🤖 AI and privacy

PhotoPrism can perform local image labeling and face recognition, and can optionally use a locally hosted vision service such as Ollama for captions and richer labels. Ghostframe does not need to send photos to a cloud AI provider. Whether any optional AI service is enabled is controlled by your PhotoPrism setup.

This initial design is **self-hosted, not end-to-end encrypted**. PhotoPrism needs readable image data to index and analyze it. Keep the server on a trusted machine/network, use HTTPS when connecting over an untrusted network, and maintain independent backups of both your originals and PhotoPrism's database/storage. E2EE is intentionally out of scope for this desktop-first build.

## 📦 Requirements

- Linux desktop with Qt 6.5 or newer (development has been started with Qt 6.11).
- `qmake6`, a C++20 compiler, and `make` to build from source.
- A reachable PhotoPrism server. The default local address is `http://127.0.0.1:2342`.
- A PhotoPrism app password with photo read access. Ghostframe stores it in an owner-only local file for automatic reconnects.
- ⚡ libvips 8.15 or newer with its C++ binding (`libvips` on Arch; `pkg-config vips-cpp` must work) for the editor. ImageMagick is no longer required.
- 🔤 Pango (pulled in by libvips) and the JetBrains Mono font (`ttf-jetbrains-mono`) for text layers; other installed fonts work too.
- 🖼️ For AVIF export, libheif with an AV1 encoder (Arch's `libheif` includes one).
- 🧬 [cybermeta](https://github.com/cybercoretech/cybermeta) **0.2 or newer** on `PATH`, `~/.local/bin` or `~/.cargo/bin` (or set `GHOSTFRAME_CYBERMETA`). It powers the metadata panel, export privacy and XMP. Without it, Ghostframe still exports, but drops all camera metadata (so a location can never leak) and writes no XMP.

## 🚀 Build and run

```sh
git clone https://github.com/cybercoretech/ghostframe.git
cd ghostframe
qmake6 ghostframe.pro
make -j"$(nproc)"
./ghostframe
```

### 🧪 Run the tests

```sh
cd tests
qmake6 && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ./tst_ghostframe
```

The suite (60 cases) generates its fixture images at runtime and runs in Qt's test mode, so it never touches your real settings or remembered edits. It covers:

- the recipe format (every parameter round-trips, unknown ops are ignored, values clamp);
- curve and tone monotonicity;
- exposure (+1 EV doubles linear light);
- white balance keeping brightness;
- vibrance favouring muted colours;
- HSL band selectivity;
- flips, straighten and perspective (no empty corners);
- region renders matching full renders;
- sharpening and noise reduction;
- clipping and histogram;
- 16-bit input staying 16-bit;
- the recipe store;
- brightness/contrast pixel values checked against ImageMagick 7.1;
- Phase 2: FX recipe round trip and hostile-input clean-up;
- looks keeping geometry and text;
- gradient map stops;
- `.cube` parsing (and errors) and LUT rendering;
- bloom, vignette, scanlines, aberration and halftone doing what they say;
- aberration at many odd frame sizes (a regression test);
- seeded effects repeating exactly;
- pixel sort ordering;
- text landing in its anchor;
- zoom regions matching full renders with FX;
- grey images becoming colour for FX;
- every export preset and format, including AVIF;
- CYBERGRID with a custom theme;
- built-in and user looks;
- an editor session with a look, text layers and a Shorts WebP export;
- Phase 3 (needs cybermeta and exiftool, otherwise skipped):
  - the cybermeta summary;
  - every privacy mode across JPG, WebP and PNG (GPS, camera identity, caption, embedded XMP, source identity only with KEEP ALL, the sidecar);
  - rotated originals exporting upright;
  - exporting without cybermeta never leaking a location;
- two full editor sessions (draft → full render, labelled history, history jumps, before frame, 1:1 zoom and pan clamping, export + XMP, edits restored on reopen);
- a speed check.

On first launch, enter your PhotoPrism URL and app password once. Ghostframe verifies the connection and saves the credential in an owner-only file in its per-user configuration directory. It restores the connection automatically on later launches, without invoking Secret Service or asking to unlock a desktop keyring. Keep your Linux account protected; the local fallback is readable only by your account.

### Edit and export a photo

Open a photo from the library and choose **✎ EDIT IMAGE**. Ghostframe downloads a temporary full-resolution working copy from PhotoPrism and opens the **Image Lab**. The right-hand panel has collapsible sections, each with its own **RESET**. Double-click any slider's label to reset just that slider.

- 📊 **Histogram:** RGB + luminance, updated with every render.
- 🔄 **Transform:** rotate, flip, straighten (the corners are cropped away automatically), vertical/horizontal perspective. **CROP** (or `R`) shows the whole frame. Drag outside the box to draw a crop, drag inside to move it, and pick an aspect: free, original, 1:1, 4:5, 3:2, 16:9 or 9:16 (Shorts). **DONE** / `Enter` applies it.
- 🌗 **Light:** exposure (EV), contrast, highlights, shadows, whites, blacks, light.
- 🎚️ **Levels:** input black, gamma, input white.
- 〰️ **Curve:** RGB, R, G and B curves over the histogram. Click to add a point, drag to move it, double-click to remove it. The line is drawn from the engine itself, so what you see is what renders.
- 🎨 **Colour:** temperature, tint, vibrance, saturation.
- 🌈 **HSL:** pick one of 8 colour bands, then shift its hue, saturation and luminance. Edited bands get a marker.
- 🔬 **Detail:** sharpen amount/radius/threshold, luminance and colour noise reduction. Check them at 1:1.
- 🕘 **History:** every step is labelled (e.g. "Exposure", "Crop", "Blue saturation"). Click one to jump there.
- 🎭 **Looks:** click a look to apply it. Your crop, geometry and text stay. **SAVE LOOK** stores the current style (everything except geometry and text) under a name. Your looks are lime, built-ins cyan.
- 🌈 **CYBERGRID gradient:** pick a Cybercore palette (your desktop's active one is preselected) and a style: NEON, SUNSET, ACID, DUO PINK, DUO CYAN or MONO. **APPLY GRADIENT** maps the image's brightness onto those colours; set the strength with **GRADIENT AMOUNT**. Custom themes in `~/.config/cybercore/themes/` show up too.
- ✨ **FX:** neon bloom (threshold, radius), chromatic aberration, glitch (with ↻ RESEED), pixel sort (rows or columns), halftone (dot size), scanlines (spacing), film grain (size), vignette (midpoint).
- 🎞️ **LUT:** **IMPORT .CUBE** copies a 3D LUT into `~/.local/share/Cybercore Systems/Ghostframe/luts/`. Pick one and set its strength. A missing LUT renders as "no LUT" with a warning.
- 🔤 **Text & watermark:** **+ TEXT** or **+ WATERMARK** ("© NEON COFFIN VIDEO", small, bottom-right). Each layer has its text, colour swatches, bold, a 3×3 anchor grid, size, margin, opacity and neon glow.
- 📤 **Export:** choose a size: ORIGINAL, SHORTS 9:16 (1080×1920), THUMB 16:9 (1280×720), SQUARE (1080) or WEB (long edge 2560, WebP). Sized presets can **FILL** (crop), **FIT + BLUR** (a blurred, darkened backdrop, great for Shorts from landscape photos) or **FIT + BARS**. Then pick AUTO, JPG, PNG, WEBP or AVIF and a quality. The file name includes the preset.
- 🧰 **Toolbar:** ‹ › previous/next photo, **CROP**, **BEFORE** (drag the split), **CLIPPING** (pink = blown highlights, cyan = crushed shadows), **1:1** (real pixels, drag to pan), **UNDO** / **REDO**.
- 💾 Edits are **remembered per photo** as you work, in `~/.local/share/Cybercore Systems/Ghostframe/recipes/`. Closing the editor or moving to another photo never loses them. **RESET ALL EDITS** (undoable) makes Ghostframe forget them.

**SAVE EXPORT + CYBERMETA** writes a new image into `~/Pictures/Ghostframe/Exports/`. Ghostframe never writes edits into the original, and it deletes the temporary working copy when you close the editor.

#### 🛡️ Metadata and privacy

| Privacy chip | Camera metadata (EXIF) in the exported file |
|---|---|
| **STRIP GPS** *(default)* | Everything except where the photo was taken |
| **ANONYMOUS** | No location, camera, lens, serials, owner (cybermeta's `anonymous-camera` preset) |
| **STRIP ALL** | No camera metadata |
| **KEEP ALL** | The original, location included (a warning appears if the photo has GPS) |

Whatever you choose:

- 🏷️ **Inside the file:** title, caption and tags as XMP (Dublin Core, which Lightroom, digiKam and darktable read), the caption also as EXIF `ImageDescription`, and the edit recipe. The PhotoPrism source identity (hash, folder, file name) is embedded only with KEEP ALL.
- 📄 **Next to the file:** a `.xmp` sidecar with everything, including the source identity and the privacy choice, for your own records.
- 🔄 **Orientation** is always written as upright, because the pixels already are.

How it works: the original's EXIF is copied into a tiny JPEG "carrier", cybermeta strips or anonymises it there, and libvips attaches the cleaned EXIF plus a cybermeta-generated XMP packet to the export. So it behaves the same for JPG, PNG, WebP and AVIF, even though cybermeta can't edit WebP EXIF itself. Every inherited metadata item (the EXIF block *and* the `exif-*` fields libvips parsed, XMP, IPTC) is dropped first, so nothing comes back on save.

### ⌨️ Keyboard shortcuts

| Where | Key | Action |
|---|---|---|
| Everywhere | `Ctrl+K` · `Ctrl+R` | Focus search · refresh the current query |
| Everywhere | `Esc` / `Q` | Step back one layer: crop → zoom → editor → viewer → settings |
| Viewer + editor | `[` / `]` | Previous / next photo in the current view |
| Editor | `Ctrl+Z` · `Ctrl+Shift+Z` / `Ctrl+Y` | Undo · redo |
| Editor | `R` · `Enter` | Crop mode on/off · apply the crop |
| Editor | `\` | Before / after split |
| Editor | `J` | Clipping warnings |
| Editor | `Z` or double-click | 1:1 zoom on/off |

Single-key shortcuts are ignored while you're typing in a text field.

### Create a read-only PhotoPrism app password

For the included Podman Compose service, create an app password tied to the PhotoPrism admin account and scoped to read photos:

```sh
podman compose -f deploy/compose.yaml exec photoprism \
  photoprism auth add -n Ghostframe -s "read" -e 31536000 admin
```

The generic `read` scope grants read-only API access across PhotoPrism resources, including the folders endpoint used by Ghostframe. Copy the app password shown by PhotoPrism into Ghostframe once; Ghostframe stores it locally for automatic reconnects. Revoke it from PhotoPrism when you no longer need Ghostframe access.

### PhotoPrism server

This repository includes a local Compose setup. It indexes `/home/raven/Pictures` read-only, keeps the PhotoPrism database and generated files under `/home/raven/Pictures/.ghostframe/`, and serves only on `127.0.0.1:2342` by default. Existing image folders remain unchanged. Start or stop the service with:

```sh
cd ~/Devspace/Cybercore/core/ghostframe
cp deploy/.env.example deploy/.env
# Replace the example passwords in deploy/.env with unique secrets before first start.
systemctl --user start podman.socket   # podman compose talks to Podman's API socket
podman compose -f deploy/compose.yaml up -d
podman compose -f deploy/compose.yaml down
```

🔁 **Start at login.** Once the containers exist, the `ghostframe-library.service` user unit starts and stops them by name (`podman start deploy-mariadb-1 deploy-photoprism-1`), so it doesn't depend on the checkout location or the API socket:

```sh
systemctl --user enable --now ghostframe-library.service
```

> 🧩 If `podman compose` reports `failed to connect to the docker API at unix:///run/user/1000/podman/podman.sock`, the socket isn't running: `systemctl --user start podman.socket` (or `enable --now` to keep it).

The `deploy/.env` file holds the generated local database and admin credentials and is excluded from Git. For a different server, follow the [official PhotoPrism installation guide](https://docs.photoprism.app/getting-started/). The app expects the API at `<server-url>/api/v1` and capability URLs for thumbnails.

The local Compose setup indexes `/home/raven/Pictures` read-only and keeps PhotoPrism's database and generated files in the hidden `/home/raven/Pictures/.ghostframe/` directory. Existing folders such as `Screenshots/2026-10-13/` stay in place; Ghostframe does not reorganize or edit your originals. The local service is read-only for now. A custom Ghostframe Android uploader is planned separately at [cybercore-tech/ghostframe-android-app](https://github.com/cybercoretech/ghostframe-android-app), with its local worktree planned for `/home/raven/APK/GhostFrame`; its upload/import path will be added once the desktop app is settled.

The current local layout is:

```text
/home/raven/Pictures/
├── Screenshots/<date>/       # existing photo folders; left in place
├── cybercore-bmc-cover.png   # existing image; left in place
├── Ghostframe/
│   ├── Inbox/                 # reserved for a future phone uploader
│   └── Exports/               # optional exports created by you
└── .ghostframe/
    ├── database/              # PhotoPrism catalog database
    └── storage/               # thumbnails, cache, and PhotoPrism settings
```

PhotoPrism scans the Pictures tree as a read-only originals library, so the existing screenshots and later Inbox uploads appear alongside each other without moving files. Keep `.ghostframe/` private and include its database and storage directories in backups; the actual photo files remain your originals.

## 🎛️ Interface direction

- **Palette:** `#080a0d` background, restrained graphite panels, electric cyan, neon pink, and acid lime.
- **Shape:** crisp one-pixel borders and compact rectangular controls.
- **Typography:** JetBrains Mono for system chrome; a readable sans-serif for image details.
- **Feel:** a cinematic local archive, with clear focus states and no visual effects that obscure the photos.
- **Behavior:** keyboard-first navigation, responsive image grid, quick search, and a full-screen viewer.

The layout takes its visual cues from the Cybercore reference interface. It uses original Ghostframe naming and UI rather than PhotoPrism branding.

## 🗺️ Roadmap

The full phase-by-phase build plan lives in **[docs/ROADMAP.md](docs/ROADMAP.md)** 🧭. Ghostframe is growing from a PhotoPrism browser into a Cybercore image system and editor:

| | Phase | What it brings |
|---|---|---|
| 🧱 | **0 · Foundation** | UI split into components, in-process **libvips** engine with a live preview proxy, JSON edit recipes, undo/redo |
| 🎛️ | **1 · Darkroom** | Exposure, curves, white balance, HSL, sharpen/denoise, straighten/perspective, histogram, before/after |
| ✨ | **2 · Cyber FX** | Neon bloom, chromatic aberration, scanlines, glitch, CYBERGRID duotones, LUTs, Shorts/thumbnail export presets |
| 🧬 | **3 · Cybermeta** | Metadata panel and GPS stripping on export through the [cybermeta](https://github.com/cybercoretech/cybermeta) CLI |
| 🗂️ | **4 · Library** | Ratings, culling, albums, compare view, batch export, Holodrive phone-inbox import |
| 🖌️ | **5 · Local edits** | Masks, heal/clone, layers |
| 🚀 | **6 · Shipping** | Live CYBERGRID theming, AUR package, project site |

Already done ✅: the Qt Quick shell and PhotoPrism client, the Cybercore theme, authenticated search and thumbnails, the nested folder browser, keyring-free auto-reconnect, and **Phase 0 · Foundation** (component UI, libvips engine, recipes, undo/redo, tests), **Phase 1 · Darkroom** (the full Image Lab above) and **Phase 2 · Cyber FX** (effects, CYBERGRID gradients, LUTs, text, looks, export presets), and **Phase 3 · Cybermeta** (metadata panel, private-by-default exports, embedded XMP and sidecars via cybermeta).

## 🛠️ Development notes

- 🤝 **Picking the project up?** Start with [`AGENT_HANDOFF.md`](AGENT_HANDOFF.md): current state, resume checklist, decisions already made, invariants and gotchas.

- 📁 Project entry point: `ghostframe.pro`. Core sources shared with the tests are listed in `ghostframe.pri`. See *Architecture* for the file map.
- 🧩 New QML component: add the file to `qml/` **and** to `qml.qrc`. Use `Theme.*` for colors and fonts, never a local id named `palette` (`Item.palette` shadows it inside delegates).
- 📜 New numeric edit parameter: add one row to the `kFields` table in `src/editrecipe.cpp` (QML key, JSON op/field, range, default), render it in `ImageEngine::apply`, add a `ParamSlider { key: … }` in `qml/Editor.qml` and a label in `ImageEditor::labelFor`, and add a test. Unknown ops are ignored when reading, so older Ghostframe builds can still open newer recipes.
- 🎭 New built-in look: add `assets/looks/<name>.json` (`{"name": …, "recipe": {"version": 1, "ops": [...]}}`) and list it in `looks.qrc`. The tests check that every built-in parses and does something.
- 🌈 CYBERGRID palettes come from the cybercore repo. Refresh them with `tools/sync-cybergrid.sh`, which also regenerates `cybergrid.qrc`; don't edit `assets/cybergrid/` by hand.
- ⏱️ Keep previews fast. Wide blurs (bloom, glow) run on a downscaled copy (`softBlur`); smooth masks (vignette) are computed small and enlarged. In libvips, a 1-band image combined with a 3-band one is broadcast per pixel and costs about 5× a matrix op. Prefer `recomb`, lookup tables (`maplut`) and per-channel operations.
- 🖼️ `GHOSTFRAME_EDIT=/path/to/image ./ghostframe` opens the editor on a local file without PhotoPrism. The file is never modified or deleted. Add `GHOSTFRAME_EDIT_VIEW=crop`, `before,clipping`, `zoom` or `scroll=N` (scrolls the adjustment panel) to start in that view (handy with `GHOSTFRAME_GRAB`).
- 📸 `GHOSTFRAME_GRAB=/tmp/shot.png[:ms]` saves a window screenshot after the delay and quits. Add `QT_QPA_PLATFORM=offscreen` for headless checks, and `QT_FORCE_STDERR_LOGGING=1` to see QML warnings.
- 🔐 The PhotoPrism token is saved in an owner-only file under the per-user configuration directory. Do not commit secrets, photo samples, or local PhotoPrism configuration.

## 📜 License

Ghostframe is distributed under the MIT License. PhotoPrism is a separate project with its own license and trademarks; using Ghostframe does not relicense or bundle PhotoPrism.

---

<sub>👻 GHOSTFRAME · CYBERCORE SYSTEMS · YOUR LIBRARY, YOUR MACHINE</sub>
