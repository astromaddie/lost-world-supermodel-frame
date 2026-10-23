# Steam Frame VR architecture

This port reuses the upstream Linux ARM64/OpenGL/OpenXR path on SteamOS. Frame-only build, display and input behavior is gated by `SUPERMODEL_FRAME`; the PC build keeps its existing defaults.

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

The ROM-free probe creates the production OpenXR instance, session, stereo OpenGL swapchains and frame loop before any ROM or Model 3 initialization. It clears the left swapchain red and the right blue, samples each FBO center pixel, submits a two-view projection layer and records `xrEndFrame` results. `-vr-probe-force-render` is probe-only: when runtime `shouldRender` is false it records that original state, uses a symmetric local-space diagnostic pose only when runtime view pose validity is incomplete, and submits a diagnostic projection. It does not modify normal gameplay rendering.

Steam Frame input is enabled when the runtime advertises `XR_VALVE_frame_controller_interaction`. The first suggested interaction profile is `/interaction_profiles/valve/frame_controller_valve`: right trigger fires, the gun-hand squeeze reloads, right A/Y starts, right B/X inserts a coin, right Menu toggles the existing options/pause panel, and left View closes an already-open panel on a short release. Holding left View for one second recentres once and pulses both controllers at amplitude 0.6 for 80 ms. The selected Frame profile uses View for recentering; the Valve Index and Touch fallback profiles retain stick-click recentering. The existing left squeeze plus left stick-click operator shortcut remains available. Index and Touch suggestions remain present if the optional Frame profile cannot be used.

Frame uses an absolute 2160x2160 target at resolution scale 1.0, bounded by each runtime's maximum image dimensions. The existing resolution-scale setting multiplies that target without multiplying SteamVR's recommendation again. Standalone defaults are scale 0.5 (1080x1080), a 72 Hz request and desktop mirroring off, based on the gameplay measurements below. Saved resolution, refresh and mirror settings take precedence. The PC defaults remain unchanged.

The opt-in `-vr-game-diagnostic` mode requires a ROM and has a finite timeout. It lets simulation and rendering continue while the runtime reports standby, uses one fixed synthetic head/hand pose source for each requested yaw (`0`, `90`, or `180` degrees), and leaves normal focus-gated gameplay unchanged. It captures both actual final eye FBOs only from a game frame at or above the configured frame (`-vr-game-diagnostic-frame=1500` by default) with at least 48 main-view models. It writes `FrameDiagnostics/eye-left-...ppm`, `eye-right-...ppm`, and a sidecar containing source game frame, model count, predicted display timestamp, eye dimensions, original `shouldRender`, whether the frame was forced, submitted layer count and successful `xrEndFrame`. The run exits unsuccessfully if it times out, submits no stereo projection, misses either eye, or records any XR/OpenGL error. Outputs go to the launcher's persistent data directory.

The ROM-free probe is bounded by `-vr-probe-seconds=1..300` and fails when no projection is submitted, a sampled eye colour is wrong, or an OpenXR/OpenGL operation reports an error. A host build or probe without an active runtime does not establish Frame display output.

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
| Initial OpenXR probe | On the Frame with the installed baseline build: 1664x1664 per eye, `GL_SRGB8_ALPHA8`, 847 submitted projection frames (1 `shouldRender=true`, 846 explicitly forced), left sample `(230,10,20,255)`, right `(10,31,235,255)`, and zero XR/OpenGL errors. The probe confirms the OpenXR eye path on that build. |
| Initial flat game | On the Frame with the installed baseline build, `-no-vr -no-throttle -no-vsync` ran for 32 seconds with 16 one-second title samples from 71.873 to 72.155 FPS, at the 72 Hz display limit; shutdown saved NVRAM normally. A separately paced run sampled about 60 FPS. |
| Current stereo game | Steam launches on the Frame captured both production eye FBOs at yaw 0, 90 and 180 degrees, each at game frame 2267 with 181 main-view models and 2160x2160 eyes. The paired images show upright scenery, distinct eye views and world geometry changing with the requested yaw. All three runs submitted stereo projection layers, returned success and recorded zero XR/OpenGL errors. |

The three stereo runs used fixed synthetic head/hand poses and explicitly forced rendering during standby. Their projection-frame counts were 4529, 4535 and 4530 respectively; each included one original `shouldRender=true` frame. This verifies the native game/render/submission path. Physical aiming, control ergonomics and optical stereo require separate interactive checks.

Separate 90-second runs used the same bounded Coin/Start stimulus and no eye readback (capture frame set to 1000000000). At 2160x2160 and a 120 Hz preference, sustained gameplay fell to roughly 33-37 display/emulation FPS with about 27-30 ms GPU time. At 1080x1080 with the mirror enabled, gameplay held 60 emulation FPS but display cadence settled near 60 FPS. With 1080x1080, the mirror disabled and SteamVR set to 72 Hz, eight five-second gameplay intervals measured 71.9-72.2 display FPS and 59.8-60.2 emulation FPS. These are opening-gameplay samples rather than a full-game performance survey. The no-capture runs intentionally reached the diagnostic timeout; all recorded zero XR/OpenGL errors.

The runtime's `predictedDisplayPeriod` sometimes continued to report 120 Hz during the 72 Hz run, while the compositor logged its 72 Hz mode selection and the app submitted about 72 frames per second. The timing log therefore labels this field `runtimePeriod`, separately from measured display and emulation cadence.

The rebuilt default profile was then launched with the original configuration restored: it selected 1080x1080 eyes, 72 Hz and no mirror without test-setting overrides. It captured game frame 4000 with 281 models, submitted 4797 stereo projection frames and exited successfully with zero XR/OpenGL errors. Before the final capture, gameplay samples remained near 72 display FPS and 60 emulation FPS.

On Mesa/Zink, querying freshly generated GPU timer-query names produced four startup `GL_INVALID_OPERATION` errors. The timer now polls only slots containing a previously ended query and consumes each available result before reuse. The same diagnostic runs with `MESA_DEBUG=context` confirmed the startup errors were eliminated.

No new menu layout or art is introduced. The existing upstream PC input and display behavior remains available outside the Frame build.
