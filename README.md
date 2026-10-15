# 👻 GHOSTFRAME

### 🖼️ A Cybercore photo archive for your desktop

**Ghostframe** is a local-first, keyboard-friendly photo browser for Omarchy and Linux desktops. It connects to your own [PhotoPrism](https://www.photoprism.app/) library and gives it a distinct Cybercore interface: near-black surfaces, sharp borders, neon cyan/pink/lime accents, mono display type, and a distraction-free image viewer.

> ⚡ **Status:** desktop MVP. The client builds and has been verified against the local PhotoPrism API and thumbnail service. Android is deliberately a later project and will live in its own repository.

## ✨ What Ghostframe is

Ghostframe is a native Qt Quick desktop client, not a rebranded PhotoPrism web build. PhotoPrism remains the photo catalog, indexer, and AI engine; Ghostframe is the custom browsing experience. Your image files stay in the storage locations you choose, and Ghostframe does not copy or edit originals.

The current client foundation includes:

- 🌌 A dark Cybercore desktop layout with a collection rail, image grid, status strip, and full-screen viewer.
- 🔎 PhotoPrism API connection, bearer-token authentication, text search, paged photo results, and a nested browser for your original folders.
- 🧭 Keyboard shortcuts: `Ctrl+K` focuses search, `Enter` runs it, `Ctrl+R` refreshes the current query, and `Esc` closes the viewer.
- 🔐 PhotoPrism credentials are restored automatically on launch. Ghostframe saves the connection in an owner-only local file and never contacts the desktop keyring, so launching it cannot trigger a keyring unlock prompt.
- 🖼️ Capability-token thumbnails served directly by PhotoPrism, without sending account credentials in image URLs.
- 🎛️ Built-in image editor with 90° rotation, drag-to-crop, light, contrast, and saturation controls. Previews are rendered quickly while exports use the original full-resolution image.
- 🧬 Cybermeta sidecars: each export gets an XMP file containing title, caption, tags, source identity, and the edit recipe. The image and metadata stay portable outside Ghostframe.
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

The interface talks to PhotoPrism's documented API. PhotoPrism keeps responsibility for indexing and AI; Ghostframe focuses on browsing. This keeps the app independent of PhotoPrism's internal database and makes it easier to track upstream changes.

## 🤖 AI and privacy

PhotoPrism can perform local image labeling and face recognition, and can optionally use a locally hosted vision service such as Ollama for captions and richer labels. Ghostframe does not need to send photos to a cloud AI provider. Whether any optional AI service is enabled is controlled by your PhotoPrism setup.

This initial design is **self-hosted, not end-to-end encrypted**. PhotoPrism needs readable image data to index and analyze it. Keep the server on a trusted machine/network, use HTTPS when connecting over an untrusted network, and maintain independent backups of both your originals and PhotoPrism's database/storage. E2EE is intentionally out of scope for this desktop-first build.

## 📦 Requirements

- Linux desktop with Qt 6.5 or newer (development has been started with Qt 6.11).
- `qmake6`, a C++20 compiler, and `make` to build from source.
- A reachable PhotoPrism server. The default local address is `http://127.0.0.1:2342`.
- A PhotoPrism app password with photo read access. Ghostframe stores it in an owner-only local file for automatic reconnects.
- ImageMagick 7 (`imagemagick`) for editor previews and full-resolution exports.

## 🚀 Build and run

```sh
git clone https://github.com/cybercore-tech/ghostframe.git
cd ghostframe
qmake6 ghostframe.pro
make -j"$(nproc)"
./ghostframe
```

On first launch, enter your PhotoPrism URL and app password once. Ghostframe verifies the connection and saves the credential in an owner-only file in its per-user configuration directory. It restores the connection automatically on later launches, without invoking Secret Service or asking to unlock a desktop keyring. Keep your Linux account protected; the local fallback is readable only by your account.

### Edit and export a photo

Open a photo from the library and choose **EDIT IMAGE**. Ghostframe downloads a temporary full-resolution working copy from PhotoPrism, then lets you rotate, crop by dragging on the preview, adjust light, contrast, and saturation, and enter title, caption, and comma-separated tags. **SAVE EXPORT + CYBERMETA** writes a new image and matching `.xmp` sidecar into `~/Pictures/Ghostframe/Exports/`. Ghostframe never writes edits into the original. The XMP sidecar includes the PhotoPrism source hash and folder plus the applied edit values, so the export can be understood and catalogued by other photo tools.

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

The local Compose setup indexes `/home/raven/Pictures` read-only and keeps PhotoPrism's database and generated files in the hidden `/home/raven/Pictures/.ghostframe/` directory. Existing folders such as `Screenshots/2026-10-13/` stay in place; Ghostframe does not reorganize or edit your originals. The local service is read-only for now. A custom Ghostframe Android uploader is planned separately at [cybercore-tech/ghostframe-android-app](https://github.com/cybercore-tech/ghostframe-android-app), with its local worktree planned for `/home/raven/APK/GhostFrame`; its upload/import path will be added once the desktop app is settled.

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
| 🧬 | **3 · Cybermeta** | Metadata panel and GPS stripping on export through the [cybermeta](https://github.com/cybercore-tech/cybermeta) CLI |
| 🗂️ | **4 · Library** | Ratings, culling, albums, compare view, batch export, Holodrive phone-inbox import |
| 🖌️ | **5 · Local edits** | Masks, heal/clone, layers |
| 🚀 | **6 · Shipping** | Live CYBERGRID theming, AUR package, project site |

Already done ✅: the Qt Quick shell and PhotoPrism client, the Cybercore theme, authenticated search and thumbnails, the nested folder browser, keyring-free auto-reconnect, and the first built-in editor.

## 🛠️ Development notes

Project entry point: `ghostframe.pro`. Qt Quick UI: `qml/Main.qml`. The API/model implementation is in `src/photoservice.*`. The authentication token is saved in an owner-only file under the per-user configuration directory for automatic reconnects. Do not commit secrets, photo samples, or local PhotoPrism configuration.

## 📜 License

Ghostframe is distributed under the MIT License. PhotoPrism is a separate project with its own license and trademarks; using Ghostframe does not relicense or bundle PhotoPrism.

---

<sub>👻 GHOSTFRAME · CYBERCORE SYSTEMS · YOUR LIBRARY, YOUR MACHINE</sub>
