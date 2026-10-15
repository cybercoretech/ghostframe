# 👻 Ghostframe Agent Handoff

The state of the project for the next agent (or person) picking it up. Keep
it current: update it in the same PR as any phase or significant change.

## 📍 Repository state (2026-10-14)

- **Repo:** `cybercore-tech/ghostframe` · local checkout `~/Devspace/Cybercore/core/ghostframe` on Blackbox.
- **`main`:** `ea53e9c` (merge of PR #8, Phase 3). No open PRs.
- **Roadmap:** [`docs/ROADMAP.md`](docs/ROADMAP.md). Phases 0–3 ✅. **Phase 4 · Library is next**; its decisions are recorded below.
- **Tests:** 60 Qt tests, all passing on `main` (the Phase 3 ones need `cybermeta` + `exiftool`; they skip without them).
- **CI:** none on GitHub yet. Tests run locally only. (Adding a workflow was offered to the user; not decided.)
- **Installed build:** `~/.local/bin/ghostframe`, built from `main`. Launcher: `~/.local/share/applications/ghostframe.desktop`.
- **Companion repo:** `cybercore-tech/cybermeta` **0.2.0** (merged PR #9, squash `4d189de`), installed at `~/.local/bin/cybermeta` from its `main`. **Not tagged or released.** Ask the user before tagging, because it publishes release binaries.

## ✅ Strict resume checklist

1. Read this file, then [`README.md`](README.md) and [`docs/ROADMAP.md`](docs/ROADMAP.md).
2. `git status` and `git log --oneline -5` on `main`; check open PRs with `GH_TOKEN=$(gh auth token -u cybercore-tech) gh pr list`.
3. Build and test before changing anything:
   ```sh
   qmake6 ghostframe.pro && make -j"$(nproc)"
   cd tests && qmake6 && make -j"$(nproc)" && QT_QPA_PLATFORM=offscreen ./tst_ghostframe
   ```
4. Work on a branch from `main`, **one PR per phase, never stacked**. The user merges.
5. **Every change updates the docs:** README (detailed, emoji section headers), `docs/ROADMAP.md` status, the Wiki page `~/Wiki/src/desktop/ghostframe.md` (mdBook; `mdbook build`, commit + push, private darkstardevx/Wiki), the vault note `~/Vaults/blackbox-notes/Projects/Ghostframe.md` (commit; no remote), and this file. Commit messages are detailed: what, why, how verified, and what wasn't verified.
6. Before committing, check that `git config user.email` is `332800418+cybercore-tech@users.noreply.github.com`. `gh`'s *active* account is darkstardevx, so always use the `GH_TOKEN=…cybercore-tech` prefix for PRs.
7. Install a new build with `install -m755 ghostframe ~/.local/bin/ghostframe`. A running window keeps the old binary; tell the user to restart.

## 🧭 Phase 4 · Library: decisions already made (2026-10-14)

The user chose all four recommended options:

- **Write access:** create a PhotoPrism app password with photos + albums **write** scope via `podman compose -f deploy/compose.yaml exec photoprism photoprism auth add …`, then store it in Ghostframe's owner-only credential file, replacing the read-only one. Nothing else is revoked. (The current token is `read` scope; a write probe returns 403.) `podman compose` needs `systemctl --user start podman.socket` first.
- **Star ratings (1–5):** PhotoPrism has none. Store them in Ghostframe per photo (like `RecipeStore`), make them filterable in the library, and write them into exports as standard `xmp:Rating`. 5★ may also set PhotoPrism *Favorite*.
- **Reject (X while culling):** mark it **in Ghostframe only** (hidden in culling/filtered views). Never touch PhotoPrism or the file; it's easy to undo.
- **Holodrive phone inbox:** **copy** new photos from `~/Shared/from-phone` into `~/Pictures/Ghostframe/Inbox/<date>/`, then have PhotoPrism index them. Leave the phone inbox untouched.
- Also in Phase 4 (roadmap): favorites/albums synced to PhotoPrism, keyboard culling (`P`/`X`/`1-5`), 2-up/4-up compare view, batch export (recipe/look → selection), and captions/labels written back to PhotoPrism.

## 🧱 Architecture (where things are)

| Area | Files | Notes |
|---|---|---|
| UI | `qml/*.qml` (+ `qml/qmldir`, `qml.qrc`) | `Main.qml` is the shell; one file per component. Add new files to `qml.qrc` and re-run `qmake6`. `Theme` singleton. |
| PhotoPrism | `src/photoservice.*` | API client, photo/folder models, `photoAt(row)` / `rowOfHash()`, original download into a temp dir. |
| Editor state | `src/imageeditor.*` | Recipe, labelled undo history, `RecipeStore`, looks, LUTs, text layers, async render/zoom/export workers with a session counter, metadata summary. |
| Pipeline | `src/imageengine.*`, `src/fx.*`, `src/cubelut.*` | Pure libvips functions, thread-safe. Order: geometry → light+tone (one integer LUT) → colour → detail → FX → text → (zoom region cut last). |
| Recipe | `src/editrecipe.*` | Versioned JSON ops. Numeric params come from the `kFields` table (QML key, JSON op/field, range, default). |
| Metadata | `src/cybermetaclient.*` | cybermeta CLI bridge: `show --summary`, EXIF privacy via a JPEG **carrier**, `xmp write`. |
| Palettes / looks | `src/cybergrid.*`, `src/looklibrary.*`, `assets/cybergrid` (synced by `tools/sync-cybergrid.sh` from `~/Devspace/Cybercore/framework/cybercore/schema/themes`), `assets/looks` | |
| Tests | `tests/tst_ghostframe.cpp`, `tests/tests.pro`, shared sources in `ghostframe.pri` | Sandboxed: `QStandardPaths::setTestModeEnabled(true)` + temp stores. **Never let tests write to real user dirs.** |

## ⚠️ Invariants and gotchas (learned the hard way)

- **Originals are never modified.** Exports are new files; downloaded working copies are deleted when the editor closes (only ones PhotoService handed over, never a user's file).
- **Privacy:** exports default to STRIP GPS. Without cybermeta, anything but KEEP drops *all* EXIF. When replacing metadata, remove the `exif-*` fields libvips parsed as well as `exif-data`, or GPS comes back on save. Exports always set `orientation=1`.
- **Brightness/contrast** use ImageMagick **7.1's** formula (`slope = 100/(100-c)` or `1+c/100`; `intercept = (b/100-0.5)*slope+0.5`), measured from `magick` output. The tan formula in older docs is wrong here.
- **libvips:**
  - Use `VImage::is_null()`, never `.width()`, on a possibly empty image (it dereferences null).
  - Use `new_from_memory_copy` for buffers that go out of scope.
  - Broadcasting 1→3 bands costs about 5×; prefer `recomb` or `maplut`.
  - Wide blurs run on a downscaled copy (`softBlur`).
- **QML:**
  - Don't name properties `palette`, `layer`, … (Item has FINAL properties with those names; `palette` silently shadows inside delegates).
  - Declare C++ context objects **before** the `QQmlApplicationEngine` in `main.cpp`.
- **moc** mis-lexes some JSON raw strings in the test file; use escaped strings. If the tests won't link (`undefined reference to vtable for TestGhostframe`), check `moc tst_ghostframe.cpp`.
- **C++:** watch for the most vexing parse (`std::vector<T> v(size_t(n))` in a declaration list).
- **little_exif** (inside cybermeta) can't edit WebP EXIF; Ghostframe works around it with the carrier.
- **Benchmarks:** the machine has background load (load average about 4–5). Repeat benchmarks before trusting them.

## 🛠️ Dev hooks

- `GHOSTFRAME_EDIT=/path/img` opens the editor on a local file (never deleted). `GHOSTFRAME_EDIT_VIEW=crop | before,clipping | zoom | scroll=N` starts in that view.
- `GHOSTFRAME_GRAB=/path.png[:ms]` saves a screenshot and quits. Use it with `QT_QPA_PLATFORM=offscreen`; add `QT_FORCE_STDERR_LOGGING=1` to see QML warnings.
- `GHOSTFRAME_CYBERMETA=/path` overrides the cybermeta binary.
- To test a heavy edit headlessly, seed `~/.local/share/Cybercore Systems/Ghostframe/recipes/local-<sha1 of abs path>.json`, and **delete it afterwards**.

## 🖥️ Machine context (Blackbox)

- PhotoPrism: Podman containers `deploy-photoprism-1` + `deploy-mariadb-1` on `127.0.0.1:2342`, started at login by `ghostframe-library.service` (user unit, `podman start` by name). Originals: `~/Pictures` (read-only mount). Catalog: `~/Pictures/.ghostframe/`.
- Credential: `~/.config/Cybercore Systems/Ghostframe/photoprism.credential` (0600; never use the keyring, per the user). `deploy/.env` holds PhotoPrism/MariaDB passwords; it's git-ignored, and backed up by MEGA by the user's choice.
- User data **not to delete**: `~/.local/share/Cybercore Systems/Ghostframe/recipes/pp-*.json` (real remembered edits).
- GPU: Radeon HD 6570 (Turks), so everything stays CPU-only.

## 📜 History

| PR | Phase / change |
|---|---|
| #1 | Keyring-free reconnect, palette-id colour fix |
| #2 | Cyan/pink card frames, `GHOSTFRAME_GRAB` |
| #3 | `docs/ROADMAP.md` |
| #4 | Checkout moved to `~/Devspace/Cybercore/core/ghostframe` |
| #5 | Phase 0 · Foundation (libvips engine, recipes, undo, QML split, tests) |
| #6 | Phase 1 · Darkroom (full Image Lab, remembered edits, crop mode, 1:1) |
| #7 | Phase 2 · Cyber FX (effects, CYBERGRID gradients, LUTs, text, looks, export presets) |
| #8 | Phase 3 · Cybermeta (metadata panel, private exports, XMP; needs cybermeta ≥ 0.2.0, cybermeta PR #9) |
