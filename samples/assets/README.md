# samples/assets

Art, audio and environment assets used by the demo games under `samples/games/` (design:
`docs/design/17_demo_games.md`). Everything in this folder except this README is **ignored by git**: the
packs are several gigabytes and are supplied by the project owner, not the repository. A fresh clone runs
every demo with generated placeholders; drop the packs below into place and re-import to get the real art.

## Expected layout

| Path | Contents | Licence |
|---|---|---|
| `Kenney Game Assets All-in-1 3.8.0/` | Kenney's all-in-one bundle, extracted as shipped: `2D assets/`, `3D assets/`, `Audio/`, `UI assets/`, `Icons/` ... | CC0 (each kit carries its own `License.txt`) |
| `HDR Skyboxes/*.hdr` | Equirectangular HDR environments (Poly Haven "puresky" set, 4K) | CC0 |

Formats the importers read directly: `.glb`/`.gltf` (every Kenney 3D kit has a `Models/GLB format/` folder),
`.png` sprites and spritesheets (with the `.xml` atlas next to them), `.ogg` audio, `.hdr` environments.
The `FBX format`, `OBJ format`, `.svg` and `.swf` variants are not used.

## Which demo uses what

| Demo | Kenney kits | Environment |
|---|---|---|
| Grid Sweeper | `2D assets/Puzzle Pack 2`, `UI assets/UI Pack`, `Audio/Interface Sounds` | - |
| Barrage | `2D assets/Particle Pack` (bullets), `2D assets/Simple Space` (ship, boss, pips, backdrop), `Audio/Digital Audio` (optional one-shots) | - |
| Colony | `2D assets/Micro Roguelike` (ants, queen, raiders, food; fallback `2D assets/Shape Characters`), `2D assets/Particle Pack` (eggs, larvae), `2D assets/Pattern Pack` (soil, surface, tunnels), `UI assets/UI Pack` (sliders, bars) | - |
| Rolling Course | `3D assets/Marble Kit`, `3D assets/Platformer Kit` (props), `Audio/Impact Sounds` | `HDR Skyboxes/kloofendal_48d_partly_cloudy_puresky_4k.hdr` |
| Corridor Strike | `3D assets/Space Station Kit`, `3D assets/Blaster Kit`, `3D assets/Blocky Characters`, `2D assets/Crosshair Pack`, `Audio/Sci-Fi Sounds` | `HDR Skyboxes/overcast_soil_puresky_4k.hdr` |

Each demo README lists its asset slots (stable asset ids) and the file each slot maps to in this folder.
