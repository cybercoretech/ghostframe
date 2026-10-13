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
- 🗝️ A session-only token field. Credentials are not written to disk by Ghostframe.
- 🖼️ Capability-token thumbnails served directly by PhotoPrism, without sending account credentials in image URLs.
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
- A PhotoPrism API access token with photo read access.

## 🚀 Build and run

```sh
git clone https://github.com/cybercore-tech/ghostframe.git
cd ghostframe
qmake6 ghostframe.pro
make -j"$(nproc)"
./ghostframe
```

On first launch, enter your PhotoPrism URL and an API access token. Ghostframe keeps the token in memory for the current session; reconnect after restarting the app.

### Create a read-only PhotoPrism app password

For the included Podman Compose service, create an app password tied to the PhotoPrism admin account and scoped to read photos:

```sh
podman compose -f deploy/compose.yaml exec photoprism \
  photoprism auth add -n Ghostframe -s "read" -e 31536000 admin
```

The generic `read` scope grants read-only API access across PhotoPrism resources, including the folders endpoint used by Ghostframe. Copy the app password shown by PhotoPrism into Ghostframe. It is used only for this session and is never stored by Ghostframe. Revoke it from PhotoPrism when you no longer need Ghostframe access.

### PhotoPrism server

This repository includes a local Compose setup. It indexes `/home/raven/Pictures` read-only, keeps the PhotoPrism database and generated files under `/home/raven/Pictures/.ghostframe/`, and serves only on `127.0.0.1:2342` by default. Existing image folders remain unchanged. Start or stop the service with:

```sh
cd /home/raven/Work/ghostframe
cp deploy/.env.example deploy/.env
# Replace the example passwords in deploy/.env with unique secrets before first start.
podman compose -f deploy/compose.yaml up -d
podman compose -f deploy/compose.yaml down
```

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

- [x] 🧩 Establish the Qt Quick desktop shell and PhotoPrism API client.
- [x] 🎨 Create the initial Cybercore theme and browse/search/viewer layout.
- [x] 🔌 Verify authenticated photo search and thumbnail loading against the local PhotoPrism instance.
- [ ] 🧪 Add robust API error states, query debounce, and image-loading refinements.
- [x] 🗂️ Browse the nested originals folders from the sidebar.
- [ ] 🗂️ Add album, people, and label browsing backed by PhotoPrism endpoints.
- [ ] 📦 Finish an Omarchy/Arch package and desktop launcher.
- [ ] 📱 Begin the separate Android uploader only after the desktop app is settled.

## 🛠️ Development notes

Project entry point: `ghostframe.pro`. Qt Quick UI: `qml/Main.qml`. The API/model implementation is in `src/photoservice.*`. The authentication token is deliberately held in process memory only. Do not commit secrets, photo samples, or local PhotoPrism configuration.

## 📜 License

Ghostframe is distributed under the MIT License. PhotoPrism is a separate project with its own license and trademarks; using Ghostframe does not relicense or bundle PhotoPrism.

---

<sub>👻 GHOSTFRAME · CYBERCORE SYSTEMS · YOUR LIBRARY, YOUR MACHINE</sub>
