# 🗺️ Ghostframe Roadmap

### 👻 From photo browser to a Cybercore image system and editor

This is the build plan agreed on 2026-10-14, extended with the express image editor implementation sequence. Each phase or named editor milestone ships as **one pull request**, and PRs are not stacked: implementation starts from `main` after the previous PR merges. Every PR also updates this file, the README, `AGENT_HANDOFF.md`, the Wiki page (`~/Wiki/src/desktop/ghostframe.md`) and the Blackbox Notes project note.

> ⚡ **Delivered:** Phases 0–3 and the handoff (PR #9). **User-selected priority:** editor layers and composition before Phase 4 library work. **Editor sequence:** 5A layers → 5B local tools → 5C curated effects → 5D scene templates → 5E optional interactive 3D/nodes. Phase 4 remains a separate library track with the decisions below preserved. No editor expansion milestone is implemented yet.

---

## 📍 Where Ghostframe is today

| Area | State |
|---|---|
| 🔎 Library | PhotoPrism browse, search, paged grid, nested folder tree. *Recently Added* and *Favorites* use PhotoPrism search filters (`order:newest`, `favorite:true`). |
| 🔐 Connection | Auto-reconnect from an owner-only local credential file; never touches the desktop keyring. |
| 🎛️ Editor | In-process libvips darkroom, Cyber FX, LUTs, text overlays, recipes, history, draft/proxy previews and 1:1 zoom. General image layers and masks are still planned. |
| 🧬 Metadata | [cybermeta](https://github.com/cybercore-tech/cybermeta) 0.2.0 reads metadata and writes embedded XMP plus sidecars. Exports strip GPS by default. |
| 📤 Export | Full-resolution render to `~/Pictures/Ghostframe/Exports/`; originals are never modified. |
| 🧱 Code | Qt Quick components in `qml/`; C++ engine, recipe, history, store and metadata bridge in `src/`. Baseline: 60 passing Qt tests. |

---

## 🧰 Engine choices

| Job | Library | Why |
|---|---|---|
| ⚡ Live preview + export pipeline | **libvips** (C++ API, linked in-process) | Streaming, multi-threaded, low memory. No process per slider move. |
| 📷 RAW / HEIC / AVIF / JXL | libvips + libraw + libheif | Camera RAW and modern formats through the same pipeline. |
| 🎞️ `.cube` LUTs | Existing C++ LUT implementation | Already delivered; no ImageMagick process required. |
| 🧪 Curated extra effects | **G’MIC / libgmic** (planned, optional) | Parameterized filters and repair/stylization operations; preview on a proxy. |
| 🩹 Selections and local repair | **OpenCV** (planned integration) | GrabCut for assisted selection; inpainting for small repairs. Clone stamp uses an explicit source offset, not inpainting. |
| 🖌️ Painting | **libmypaint** (planned integration) | Brush strokes on editable surfaces; Ghostframe owns input, masks and history. |
| 🧊 Scene templates | **Blender Python API + background worker** (planned, optional) | Render titles, logos and mockups into image layers, with CPU Cycles as the initial target. |
| 🎥 Interactive 3D | **Filament** (later evaluation) | Native rendering and glTF loading; custom Qt integration and hardware validation required. |
| 🧬 EXIF / XMP / privacy | **cybermeta** CLI (JSON) | Our own tool becomes Ghostframe's metadata core. |

**🖥️ CPU editing first.** Blackbox's Radeon HD 6570 (Turks) already needs GPU compositing workarounds in Chromium apps. Keep image editing on libvips with draft/proxy previews and full-resolution export/1:1 rendering. Initial Blender scene jobs use CPU rendering and stay optional. Interactive GPU 3D is a later capability check, not a new requirement for opening or editing photos.

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

> ✅ **Delivered (2026-10-14):**
> - Every control listed above.
> - The 3:2 crop aspect was added.
> - Light and tone are fused into one per-channel lookup table on the integer pixels.
> - Saturation is a single colour matrix.
> - Drags render an 800 px draft and full quality comes on release.
> - Previews run about 7 ms for light/tone and about 150 ms with every stage active, on a 1600 px proxy.
>
> **Beyond the plan:**
> - Per-photo **remembered edits** (`RecipeStore`), which makes `[` / `]` navigation lossless.
> - **Crop mode**, which shows the whole frame with a movable, aspect-locked box.
> - A labelled history.
> - `GHOSTFRAME_EDIT_VIEW` for headless screenshots of each view.
> - 37 tests.

## ✨ Phase 2: Cyber FX (signature looks)

- 🌈 Neon bloom / glow, chromatic aberration, scanlines, glitch and pixel sort, film grain, vignette, halftone.
- 🎨 Duotone and gradient maps generated from the **CYBERGRID** theme palettes.
- 🎞️ LUT support (`.cube`), plus user presets ("looks") saved as recipe fragments.
- 🔤 Text and watermark layers (JetBrains Mono, theme colors).
- 📐 **Export presets:** YouTube Shorts 1080×1920, thumbnail 1280×720, square 1080, and web WebP/AVIF at chosen quality. Made for Neon Coffin Video work.

> ✅ **Delivered (2026-10-14):**
> - All effects listed above.
> - CYBERGRID gradient maps from the 81 bundled palettes (synced by `tools/sync-cybergrid.sh`) plus custom themes, in 6 styles.
> - `.cube` LUT import and library.
> - Text/watermark layers with anchors and neon glow.
> - 9 built-in looks plus save/delete for your own.
> - Export presets with fill / fit+blur / fit+bars and JPG/PNG/WebP/AVIF.
> - 51 tests.
>
> **Changes from the plan:**
> - Duotones are styles of the gradient map (DUO PINK / DUO CYAN) rather than a separate op.
> - 1:1 zoom now cuts its region after the whole pipeline, so FX stay exact.

## 🧬 Phase 3: Cybermeta integration

- 🪪 **Metadata panel** in the viewer: camera, lens, exposure, dates, GPS (with a map link), from `cybermeta show <file> --format json`.
- 🛡️ **Privacy on export:** *strip GPS* on by default, optional *anonymous camera* preset (`cybermeta strip` / `spoof`).
- ✍️ Title, caption and tags written into the exported file's EXIF/XMP, not only the sidecar.
- 🔧 **cybermeta repo (its own PR):** an XMP sidecar command and the `cybermeta:` recipe namespace, so Ghostframe's hand-written XMP writer moves into cybermeta as the single source of truth. Install cybermeta on Blackbox.

> ✅ **Delivered (2026-10-14):**
> - **cybermeta 0.2.0 (cybermeta PR #9):**
>   - `show --summary` (typed camera/exposure/time/GPS JSON with a map link);
>   - `xmp write|read` with the `cybermeta:` namespace and recipe;
>   - reading XMP from JPEG/WebP/PNG, including libvips' compressed `zTXt`.
> - **Ghostframe (#8):**
>   - a METADATA panel;
>   - privacy chips with STRIP GPS by default;
>   - EXIF cleaned via a JPEG carrier, so it works for every format;
>   - XMP embedded in the file and a sidecar, both from cybermeta, replacing the internal XML writer;
>   - ImageDescription set from the caption;
>   - Orientation always 1;
>   - a safe fallback without cybermeta.
> - **Found along the way:**
>   - libvips re-applies parsed `exif-*` fields on save, so they are now removed explicitly;
>   - exports with kept EXIF carried a stale Orientation (now fixed for every export);
>   - little_exif can't edit WebP EXIF (documented in cybermeta).
> - 60 tests.

## 🗂️ Phase 4: Library power

- ⭐ Favorites sync to PhotoPrism. Ratings (`1`–`5`) and pick/reject (`P` / `X`) are Ghostframe-local state for keyboard culling; rejects never change PhotoPrism or source files. Export ratings as standard `xmp:Rating`. Five stars may also set Favorite.
- 📚 Albums: browse, create, add/remove photos.
- 🪟 Compare view (2-up / 4-up).
- 🏭 Batch export: apply a recipe or look to a selection, resize, convert format.
- 📝 Write captions and labels back to PhotoPrism.
- 📱 **Holodrive tie-in:** copy new photos from `~/Shared/from-phone` to `~/Pictures/Ghostframe/Inbox/<date>/`, then ask PhotoPrism to index the copies. Leave the phone inbox untouched.

**Connection preparation is complete:** the prior Claude session reports that the "Ghostframe (library write)" app password was created, saved and verified with a favorite/unfavorite round trip. The old read-only password remains valid and backed up. Do not create another password as a routine resume step. The library UI features above are not implemented yet.

## 🖌️ Phase 5: Express editor — Edit, Compose, Scene

The product direction is a compact GIMP/Blender-style workflow: edit a photo, compose image/text elements, and insert rendered 3D titles or mockups. Retain Qt Quick, libvips, original-file safety and the existing darkroom tools. These are planned capabilities, not installed features.

### 🧅 5A · Document and layer foundation

- A versioned document model with stable layer IDs, image/text/group/adjustment layers, ordering, visibility, opacity and blend modes.
- Per-layer position, scale, rotation and flips, with consistent coordinates across preview and export; mask attachments and group composition rules.
- Import images into managed project assets so edits survive renaming or removing the imported source. Define missing-asset recovery and portable project packaging; do not hide missing inputs during export.
- Preserve existing v1 recipes and text overlays through explicit migration. Keep a shared rendering path for preview, zoom and export.
- A layer panel and canvas selection/transform controls, integrated with labelled undo/redo and remembered edits. Existing Looks must preserve composition content.
- Define RGBA/alpha handling, working color space and compositing order before adding render operations. Build on libvips compositing.

**Done when:** a user can combine two images and text, group/reorder them, apply a masked adjustment, undo/redo, reopen the document, and export it without changing originals. Tests cover migration, blend/alpha pixels, masks, group order, asset failures, persistence and preview/export placement. Verify the QML controls in a headless UI session.

### 🎭 5B · Selections, masks and local tools

- Rectangle/lasso selections, feather/invert, brush masks, and linear/radial masks with local adjustments.
- OpenCV GrabCut for assisted foreground extraction with corrective strokes; small-area healing through inpainting.
- Clone stamp with a selectable source point and offset; keep clone and heal as separate tools.
- libmypaint for painting/erasing with tablet pressure when available. Use tile updates or bounded regions and stroke-level history so drawing does not rerender the full document on every pointer event.

**Done when:** a user can isolate a subject, refine its edge, paint a local adjustment, clone a region and heal a small blemish. Verify stroke undo, zoom-coordinate mapping, mask persistence and unchanged pixels outside the edited region. Large generative object removal is outside this milestone.

### 🧪 5C · Curated G’MIC effects

- An optional libgmic adapter or background CLI worker, a curated catalog, parameter controls and cached proxy previews.
- Store filter identity, parameters, seeds and engine/definition version with the document. Pin filter definitions used by saved edits.
- Support progress, cancellation, error reporting and missing-engine behavior without blocking basic editing. Export must report unavailable required effects rather than silently omit them.
- Verify the licenses of the chosen library/CLI components and filter definitions before packaging them; do not assume the G’MIC-Qt plugin is a drop-in QML component.

**Done when:** selected effects remain editable after reopening and render consistently at export resolution; tests cover repeatability, cancellation, unavailable dependencies and saved parameters.

### 🧊 5D · Blender scene templates

- Run Blender as an optional background process controlled by its Python API. Start with extruded titles, logo scenes and product mockups with a small set of camera/material/light controls.
- Retain the editable scene parameters and assets, and composite a transparent render as a scene layer. Keep its cached render so documents can still be viewed when Blender is absent; mark stale renders explicitly.
- CPU Cycles first, with low-cost preview settings, job progress/cancellation and full-quality export rendering. Do not run Blender on each slider movement.
- Define alpha and color conversion between scene renders and libvips. Keep source scenes distinct from exported raster images.

**Done when:** a user inserts a 3D title, changes its material/camera, cancels a render, reopens it for editing and exports the composition. Test missing Blender, failed/cancelled jobs, cache invalidation, transparent edges and document recovery. Full mesh modeling/sculpting is outside this milestone.

### 🎥 5E · Optional interactive 3D and node editing

- Evaluate Filament for a native glTF scene viewport only after measuring compatibility and responsiveness on the target machine. Keep a usable CPU/template workflow when GPU rendering is unavailable.
- Build selection, transform gizmos, camera and lighting controls around the chosen renderer; a rendering engine does not supply a complete editor.
- If needed, expose the existing document operations as a node graph with cycle validation, serialization and the same undo/render model. A node UI alone is not the processing engine.
- GEGL is an alternative graph-processing engine to evaluate, not an additional mandatory pipeline. Qt Quick 3D is another integration option, but its GPLv3/commercial licensing differs from the current MIT application and requires an explicit distribution decision.

**Done when:** the hardware evaluation and integration decision are documented, and any shipped viewport/graph preserves the same document semantics and export behavior. This milestone remains exploratory; it does not block 5A–5D or shipping the CPU editor.

### 🔗 Upstream references

- [libvips compositing](https://www.libvips.org/API/current/type_func.Image.composite.html)
- [OpenCV GrabCut](https://docs.opencv.org/4.x/d8/d83/tutorial_py_grabcut.html) and [inpainting](https://docs.opencv.org/4.x/df/d3d/tutorial_py_inpainting.html)
- [libmypaint](https://github.com/mypaint/libmypaint), [G’MIC / libgmic](https://gmic.eu/)
- [Blender background rendering](https://docs.blender.org/manual/en/3.6/advanced/command_line/render.html)
- [Filament](https://github.com/google/filament), [Qt Quick 3D](https://doc.qt.io/qt-6/qtquick3d-index.html), [GEGL](https://gegl.org/)

## 🚀 Phase 6: Polish and shipping

- 🎨 Follow the active CYBERGRID theme live, like the other Cybercore apps.
- 📦 AUR package (`ghostframe`, `ghostframe-bin`) using the cyberterm packaging pattern.
- 🌐 Project site in the shared Cybercore Pages kit.

---

## 🧾 Status

| Phase | Status | PR |
|---|---|---|
| 0 · Foundation | ✅ done | #5 |
| 1 · Darkroom | ✅ done | #6 |
| 2 · Cyber FX | ✅ done | #7 |
| 3 · Cybermeta | ✅ done | #8 (+ cybermeta #9) |
| Handoff | ✅ merged | #9 |
| 4 · Library | 🕓 planned; write access prepared | — |
| 5A · Document and layers | 🕓 first editor milestone | — |
| 5B · Selections and local tools | 🕓 after 5A | — |
| 5C · Curated G’MIC effects | 🕓 after 5B | — |
| 5D · Blender scene templates | 🕓 after 5C | — |
| 5E · Interactive 3D / nodes | 🔬 optional evaluation after 5D | — |
| 6 · Shipping | 🕓 planned | — |
