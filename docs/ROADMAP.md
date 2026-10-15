# 🗺️ Ghostframe Roadmap

### 👻 From photo browser to a Cybercore image system and editor

This is the build plan agreed on 2026-10-14. Each phase ships as **one pull request**, and PRs are not stacked: a phase starts from `main` after the previous one merges. Every phase PR also updates this file, the README, the Wiki page (`~/Wiki/src/desktop/ghostframe.md`) and the Blackbox Notes project note.

> ⚡ **Order:** Phase 0 ✅ → Phase 1 first (make the editor feel great), then Cyber FX, Cybermeta, library features, local edits, packaging.

---

## 📍 Where Ghostframe is today

| Area | State |
|---|---|
| 🔎 Library | PhotoPrism browse, search, paged grid, nested folder tree. *Recently Added* and *Favorites* use PhotoPrism search filters (`order:newest`, `favorite:true`). |
| 🔐 Connection | Auto-reconnect from an owner-only local credential file; never touches the desktop keyring. |
| 🎛️ Editor | 90° rotate, drag-crop, light / contrast / saturation. Every preview runs a separate `magick` process, so sliders lag. |
| 🧬 Metadata | Ghostframe writes its own XMP sidecar (`cybermeta:` namespace, title / caption / tags / recipe). It does **not** use the [cybermeta](https://github.com/cybercore-tech/cybermeta) tool yet. |
| 📤 Export | Full-resolution render to `~/Pictures/Ghostframe/Exports/`; originals are never modified. |
| 🧱 Code | All UI in one `qml/Main.qml` (~420 lines, long one-line components). |

---

## 🧰 Engine choices

| Job | Library | Why |
|---|---|---|
| ⚡ Live preview + export pipeline | **libvips** (C++ API, linked in-process) | Streaming, multi-threaded, low memory. No process per slider move. |
| 📷 RAW / HEIC / AVIF / JXL | libvips + libraw + libheif | Camera RAW and modern formats through the same pipeline. |
| 🧪 Rare effects, `.cube` / Hald LUTs | ImageMagick 7 | Huge filter catalog for heavier one-off operations. |
| 🩹 Heal, clone, auto-straighten | OpenCV | Inpainting, edge and line detection. |
| 🧬 EXIF / XMP / privacy | **cybermeta** CLI (JSON) | Our own tool becomes Ghostframe's metadata core. |

**🖥️ CPU, not GPU.** Blackbox's Radeon HD 6570 (Turks) already needs GPU compositing workarounds in Chromium apps. Editing stays on the CPU: libvips on 12 cores, working on a ~1600 px *preview proxy*, keeps sliders live. Full-resolution rendering happens only on export.

**🔗 Cybermeta integration = CLI first.** Ghostframe calls `cybermeta … --format json` through `QProcess`. Linking cybermeta as a Rust library over a C ABI can come later if the CLI becomes a bottleneck.

---

## 🧱 Phase 0: Foundation

*Goal: an architecture that a real editor can grow on.*

- 🧩 **Split the UI.** `qml/Main.qml` → `qml/components/` (`Sidebar`, `PhotoGrid`, `PhotoCard`, `Viewer`, `Editor/*`, `SettingsPanel`, `HeaderButton`, `SideItem`) plus a shared `Theme` singleton.
- ⚡ **`ImageEngine` on libvips.** It runs in-process on a `QThreadPool`. It builds a preview proxy once per open image, and each slider change re-renders only the proxy. Frames reach QML through a `QQuickImageProvider` (no temp PNG files). Requests are coalesced, so only the newest render wins.
- 📜 **Edit recipe.** A versioned JSON list of operations (`{"op":"exposure","ev":0.3}` …) that is the single source of truth. Preview and export run the same recipe; export runs it on the original at full resolution.
- ↩️ **Undo / redo** as a history stack over recipe states (`Ctrl+Z` / `Ctrl+Shift+Z`).
- 🧪 **Tests.** A Qt Test target that runs recipes on small fixture images and compares output hashes and dimensions. Headless UI check through `GHOSTFRAME_GRAB`.
- 📦 `ghostframe.pro` links `vips-cpp` via `pkg-config`; the README requirements are updated.

**Done when:** the current editor features run on the new engine, preview updates in well under 100 ms on the proxy, ImageMagick is no longer required for basic editing, and undo/redo works.

> ✅ **Delivered (2026-10-14):**
> - 16 QML files plus a `Theme` singleton.
> - `ImageEngine` on libvips, with a ~15 ms preview render on a proxy of a 6000×4000 image.
> - Recipe v1 (`rotate`, `crop`, `brightnessContrast`, `saturation`) and undo/redo; one slider drag is one step.
> - 21 Qt tests, including pixel values checked against ImageMagick 7.1 and a full editor-to-export session.
> - ImageMagick is no longer a dependency.
> - Also fixed: cropping again on an already-cropped preview now maps to the right region, and downloaded working copies are deleted when the editor closes.

## 🎛️ Phase 1: Darkroom (the editor)

*Goal: an editor that feels good to use.*

- 🌗 **Tone:** exposure, highlights, shadows, whites, blacks, a point curve (RGB + per channel), levels.
- 🎨 **Color:** white balance (temperature / tint), vibrance, saturation, HSL per color band (8 bands).
- 🔬 **Detail:** sharpen (amount / radius / threshold), luminance + color noise reduction.
- 📐 **Geometry:** crop with aspect presets (free, 1:1, 4:5, 16:9, 9:16), straighten slider, flip H/V, perspective (keystone).
- 📊 **Tools:** live RGB histogram, highlight/shadow clipping overlay, before/after split (drag the divider), 1:1 zoom and pan, a clickable history list.
- ⌨️ **Keys:** `\` before/after, `R` crop, `Ctrl+Z` undo, `[`/`]` previous/next photo.

**Done when:** a typical photo can be fully graded in Ghostframe and exported, with the edit stored as a recipe that reopens editable.

## ✨ Phase 2: Cyber FX (signature looks)

- 🌈 Neon bloom / glow, chromatic aberration, scanlines, glitch and pixel sort, film grain, vignette, halftone.
- 🎨 Duotone and gradient maps generated from the **CYBERGRID** theme palettes.
- 🎞️ LUT support (`.cube`), plus user presets ("looks") saved as recipe fragments.
- 🔤 Text and watermark layers (JetBrains Mono, theme colors).
- 📐 **Export presets:** YouTube Shorts 1080×1920, thumbnail 1280×720, square 1080, and web WebP/AVIF at chosen quality. Made for Neon Coffin Video work.

## 🧬 Phase 3: Cybermeta integration

- 🪪 **Metadata panel** in the viewer: camera, lens, exposure, dates, GPS (with a map link), from `cybermeta show <file> --format json`.
- 🛡️ **Privacy on export:** *strip GPS* on by default, optional *anonymous camera* preset (`cybermeta strip` / `spoof`).
- ✍️ Title, caption and tags written into the exported file's EXIF/XMP, not only the sidecar.
- 🔧 **cybermeta repo (its own PR):** an XMP sidecar command and the `cybermeta:` recipe namespace, so Ghostframe's hand-written XMP writer moves into cybermeta as the single source of truth. Install cybermeta on Blackbox.

## 🗂️ Phase 4: Library power

- ⭐ Set favorites, ratings (`1`–`5`) and pick/reject (`P` / `X`) from Ghostframe, synced to PhotoPrism. Keyboard culling mode.
- 📚 Albums: browse, create, add/remove photos.
- 🪟 Compare view (2-up / 4-up).
- 🏭 Batch export: apply a recipe or look to a selection, resize, convert format.
- 📝 Write captions and labels back to PhotoPrism.
- 📱 **Holodrive tie-in:** photos dropped in `~/Shared/from-phone` are imported into PhotoPrism automatically.

## 🖌️ Phase 5: Local edits

- 🎭 Brush, linear and radial gradient masks with per-mask adjustments.
- 🩹 Heal and clone (OpenCV inpainting).
- 🧅 Layers with blend modes and opacity.

## 🚀 Phase 6: Polish and shipping

- 🎨 Follow the active CYBERGRID theme live, like the other Cybercore apps.
- 📦 AUR package (`ghostframe`, `ghostframe-bin`) using the cyberterm packaging pattern.
- 🌐 Project site in the shared Cybercore Pages kit.

---

## 🧾 Status

| Phase | Status | PR |
|---|---|---|
| 0 · Foundation | ✅ done | #5 |
| 1 · Darkroom | ⏳ next | — |
| 2 · Cyber FX | 🕓 planned | — |
| 3 · Cybermeta | 🕓 planned | — |
| 4 · Library | 🕓 planned | — |
| 5 · Local edits | 🕓 planned | — |
| 6 · Shipping | 🕓 planned | — |
