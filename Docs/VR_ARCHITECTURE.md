# Steam Frame VR architecture

This port reuses the upstream Linux ARM64/OpenGL/OpenXR path on SteamOS. Frame-specific compiler flags and the probe are enabled only by `SUPERMODEL_FRAME`; the ordinary PC build keeps its existing defaults.

## Current renderer inventory

| Draw path | Current VR rule |
| --- | --- |
| New3D scene | Retained scene is drawn once per eye. Runtime eye FOV and pose drive the asymmetric projection. |
| Main-only views | Viewports marked main-only keep their existing per-eye visibility rules. |
| Spotlight ellipse | Its pixel-space ellipse is remapped for each eye viewport. |
| Lighting | The New3D VR shader applies each eye camera transform and the existing lighting inputs. |
| Flat/menu viewports | Small flat viewports use `DrawVRSubViewports`; they are not treated as stereo world geometry. |
| HUD | Top-layer alpha is derived in the shader; parked/hidden model rules remove letterbox and cutscene sight. |
| Backdrop | The 4:3 background is mirrored beyond its original horizontal edges. |
| Light-gun hit test | The existing flat pass remains authoritative. VR aim uses a one-frame PBO result collected in `CollectLos`. |
| Menu and gun overlays | Existing OpenXR paths draw them after the game scene; the colour probe bypasses these paths. |

The probe creates the production OpenXR instance, session, stereo OpenGL swapchains and frame loop before any ROM or Model 3 initialization. It clears the left swapchain red and the right blue, samples each FBO center pixel, submits a two-view projection layer and records `xrEndFrame` results. `-vr-probe-force-render` is explicitly probe-only: when runtime `shouldRender` is false it records that original state, uses a symmetric local-space diagnostic pose only when runtime view pose validity is incomplete, and submits a diagnostic projection. It does not modify gameplay rendering. Runtime-provided eye separation is retained when position data is valid.

The probe is bounded by `-vr-probe-seconds=1..300` and fails when no projection is submitted, a sampled eye colour is wrong, or an OpenXR/OpenGL operation reports an error. A host build or probe without an active runtime does not establish Frame display output.

## Reproduction

The native target builds from the ARM64 `ubuntu:22.04` base image. `Scripts/frame/Dockerfile` installs the pinned Jammy compiler, OpenGL, SDL2, SDL2_net and OpenXR development packages plus host verification tools; it does not depend on another repository's image:

```sh
Scripts/frame/build.sh
```

The build uses two jobs, creates separate `objframe/` and `binframe/` outputs, and writes the portable archive under `dist/`. Re-running the packaging step stages and verifies both outputs before replacing the previous generated package. No ROM, NVRAM, custom gun model or menu artwork is included. `run-frame.sh --probe 30 --force-render` runs the bounded ROM-free diagnostic. Normal launch selects `ROMs/lostwsga.zip` in the persistent data tree or accepts a ROM path explicitly, resolved against the launcher's caller directory. `--flat` provides the same persistent-data launcher with the Frame-only `-no-vr` config override.

## Evidence scope

| Gate | Evidence |
| --- | --- |
| Source and package | Frame-only build target, explicit package allowlist, ABI/dependency checks, checksums and build metadata. |
| Host behavior | Full ARM64 build, command-line checks, Xvfb OpenGL information and launcher tests. |
| OpenXR probe | Requires a runtime session and submitted stereo projection; report runtime `shouldRender`, forced-frame count, sampled left/right pixels, layer count and `xrEndFrame` result. |
| Flat game | Requires a bounded on-device run using the user's ROM from persistent storage. |
| Stereo game | Requires separate per-eye gameplay verification after the probe and flat gates. |

No game camera, controller mapping, menu layout or control labels are changed by this foundation.
