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
| Backdrop | The 4:3 background is mirrored beyond both axes at infinity, with rotation only; its uploaded texture persists across retained-scene redraws. |
| Light-gun hit test | The existing flat pass remains authoritative. VR aim uses a one-frame PBO result collected in `CollectLos`. |
| Menu and gun overlays | Existing OpenXR paths draw them after the game scene; the colour probe bypasses these paths. |

The ROM-free probe creates the production OpenXR instance, session, stereo OpenGL swapchains and frame loop before any ROM or Model 3 initialization. It clears the left swapchain red and the right blue, samples each FBO center pixel, submits a two-view projection layer and records `xrEndFrame` results. `-vr-probe-force-render` is probe-only: when runtime `shouldRender` is false it records that original state, uses a symmetric local-space diagnostic pose only when runtime view pose validity is incomplete, and submits a diagnostic projection. It does not modify normal gameplay rendering.

Steam Frame input is enabled when the runtime advertises `XR_VALVE_frame_controller_interaction`. The first suggested interaction profile is `/interaction_profiles/valve/frame_controller_valve`: right trigger fires, the gun-hand squeeze reloads, right A/Y starts, right B/X inserts a coin, right Menu toggles the existing options/pause panel, and left View closes an already-open panel on a short release. Holding left View for one second recentres once and pulses both controllers at amplitude 0.6 for 80 ms. The selected Frame profile uses View for recentering; the Valve Index and Touch fallback profiles retain stick-click recentering. The existing left squeeze plus left stick-click operator shortcut remains available. Index and Touch suggestions remain present if the optional Frame profile cannot be used.

Frame uses an absolute 2160x2160 target at resolution scale 1.0, bounded by each runtime's maximum image dimensions. The existing resolution-scale setting multiplies that target without multiplying SteamVR's recommendation again. Standalone defaults are scale 0.5 (1080x1080), a 72 Hz request and desktop mirroring off, based on the gameplay measurements below. Saved resolution, refresh and mirror settings take precedence. The PC defaults remain unchanged.

The opt-in `-vr-game-diagnostic` mode requires a ROM and has a finite timeout. It lets simulation and rendering continue while the runtime reports standby, uses one fixed synthetic head/hand pose source for each requested yaw (`0`, `90`, or `180` degrees), and leaves normal focus-gated gameplay unchanged. It captures both actual final eye FBOs only from a game frame at or above the configured frame (`-vr-game-diagnostic-frame=1500` by default) with at least 48 main-view models. It writes `FrameDiagnostics/eye-left-...ppm`, `eye-right-...ppm`, and a sidecar containing source game frame, model count, predicted display timestamp, eye dimensions, original `shouldRender`, whether the frame was forced, submitted layer count and successful `xrEndFrame`. The run exits unsuccessfully if it times out, submits no stereo projection, misses either eye, or records any XR/OpenGL error. Outputs go to the launcher's persistent data directory.

The ROM-free probe is bounded by `-vr-probe-seconds=1..300` and fails when no projection is submitted, a sampled eye colour is wrong, or an OpenXR/OpenGL operation reports an error. A host build or probe without an active runtime does not establish Frame display output.

## Aim, held gun, and submission

The runtime's `aim/pose` supplies local -Z forward and +Y up. One controller-local
fine-adjustment quaternion drives both the held model and menu/game rays; there is
no borrowed grip-axis pitch correction. Head and hands are located at the same
predicted display time, and rotation-only mode subtracts the same head position
from the game ray and held model. Frame keeps the configured gun hand when its
tracking is unavailable and suppresses firing at a stale target. Pointer rendering
reprojects the stored predicted ray against the current frustum and panel depth/size,
including its visibility bounds. This keeps the pointer on the barrel axis through
camera zooms while leaving the emulator's already-consumed input snapshot unchanged.

Lost World's wide calibration and input conversion are paired in code, including
on installations without the old PC settings file. With wide calibration enabled,
default X scale/offset are `0.5497 / 0.2162`, and Y are `0.5507 / 0.2155`; explicit
settings still override them. With calibration disabled, defaults are identity.
These are the existing PC calibration values, not an independently remeasured
ROM hit response. `test-controller-aim.cpp` covers production quaternion/ray math,
rolled hands, screen center/edges, depth, translation modes, and calibration values.

The Game gun slot loads a private `VRGameGun.obj` first. If absent, the Frame Lost
World build loads `FrameArcadeGun.obj`: the approved 17,108-triangle Namco Arcade
Gun by Martoscar, CC-BY-4.0. The importer retains topology, normals, scale and
orientation, samples its TC2 atlas into face materials, and translates its optical
emitter center onto the OpenXR aim ray. The physical muzzle and ray now share
local X/Y zero; the optical lens geometry itself is checked by
`test-arcade-gun.py`. Blue retains the asset's material colors; Red recolors only
blue plastic. Custom models and saved model/off choices remain supported.

On Frame, every eye is flushed after drawing; all acquired images remain owned
until one `glFinish` in `EndFrame`, followed by the swapchain releases. This protects
the GL producer/compositor handoff without serializing each eye separately.
Retained-scene redraws no longer advance the gameplay/HUD debounce counters.
Submission correctness, GPU cost, and visible flicker must be checked separately.

### Focused diagnostics

Only `-vr-game-diagnostic` reads these environment controls:

- `SM_FRAME_DIAG_SEQUENCE=1..16`: capture this many qualified display frames.
- `SM_FRAME_DIAG_HOLD_SCENE=1`: after the first capture, drain pending CPU work and
  redraw the same scene/textures for the remaining samples.
- `SM_FRAME_DIAG_HAND_YAW`, `SM_FRAME_DIAG_HAND_PITCH`, `SM_FRAME_DIAG_HAND_ROLL`:
  rotate synthetic hands relative to the head, in degrees. Defaults are zero.

Every sample writes each eye immediately after the backdrop and at final output,
with an ordinal, source frame, new/redraw flag, pose/ray, gun vertex counts, and
submission metadata. The first sample reads each unchanged FBO twice and fails
on disagreement. The default remains one sample. These readbacks force GPU work
and alter scheduling; they do not establish that ordinary presentation is free of
flicker. Use the hold control to distinguish retained-state changes from animation.

`SM_FRAME_DIAG_SKIP_GPU_WAIT=1` disables completion only in the color probe or game
diagnostic. For matched no-readback timing, set the capture frame to `1000000000`
and a bounded timeout, hold resolution/mirror/refresh/input stimulus constant,
and compare wait on/off in the same binary. Timeout is the expected exit in that
case. A color-probe A/B isolates submission from the game's backdrop. Normal
launches always keep the completion wait, even if that environment key is present.

For eye captures use the existing Steam-launched diagnostic at the selected game
frame with `SM_FRAME_DIAG_SEQUENCE=8`, first with a live scene, then with
`SM_FRAME_DIAG_HOLD_SCENE=1`. Repeat at the existing 0/90/180 head yaws and use hand
yaw/pitch/roll combinations for the visible gun. Inspect paired backdrop/final
stages, check unchanged-source readbacks, and keep captured correctness runs
separate from timing and physical controller acceptance. Reduce images to compact
evidence grids and remove full-resolution development captures after review.

## Reproduction

The native target builds from the ARM64 `ubuntu:22.04` base image. `Scripts/frame/Dockerfile` installs the pinned Jammy compiler, OpenGL, SDL2, SDL2_net and OpenXR development packages plus host verification tools; it does not depend on another repository's image:

```sh
Scripts/frame/build.sh
```

The build uses two jobs, creates separate `objframe/` and `binframe/` outputs, and writes the portable archive under `dist/`. Re-running the packaging step stages and verifies both outputs before replacing the previous generated package. No ROM, NVRAM, private gun model or menu artwork is included. The independently created CC-BY-4.0 Namco Arcade Gun by Martoscar is included with its attribution and conversion manifest. `run-frame.sh --probe 30 --force-render` runs the bounded ROM-free diagnostic. Normal launch selects `ROMs/lostwsga.zip` in the persistent data tree or accepts a ROM path explicitly, resolved against the launcher's caller directory. `--flat` provides the same persistent-data launcher with the Frame-only `-no-vr` config override.

## Evidence scope

| Gate | Evidence |
| --- | --- |
| Source and package | Frame-only build target, explicit package allowlist, ABI/dependency checks, checksums and build metadata. |
| Host behavior | Full ARM64 build, command-line checks, Xvfb OpenGL information and launcher tests. |
| Initial OpenXR probe | On the Frame with the installed baseline build: 1664x1664 per eye, `GL_SRGB8_ALPHA8`, 847 submitted projection frames (1 `shouldRender=true`, 846 explicitly forced), left sample `(230,10,20,255)`, right `(10,31,235,255)`, and zero XR/OpenGL errors. The probe confirms the OpenXR eye path on that build. |
| Initial flat game | On the Frame with the installed baseline build, `-no-vr -no-throttle -no-vsync` ran for 32 seconds with 16 one-second title samples from 71.873 to 72.155 FPS, at the 72 Hz display limit; shutdown saved NVRAM normally. A separately paced run sampled about 60 FPS. |
| Baseline stereo game | Steam launches on the Frame captured both production eye FBOs at yaw 0, 90 and 180 degrees, each at game frame 2267 with 181 main-view models and 2160x2160 eyes. The paired images show upright scenery, distinct eye views and world geometry changing with the requested yaw. All three runs submitted stereo projection layers, returned success and recorded zero XR/OpenGL errors. |

The three stereo runs used fixed synthetic head/hand poses and explicitly forced rendering during standby. Their projection-frame counts were 4529, 4535 and 4530 respectively; each included one original `shouldRender=true` frame. This verifies the native game/render/submission path. Physical aiming, control ergonomics and optical stereo require separate interactive checks.

Separate 90-second runs used the same bounded Coin/Start stimulus and no eye readback (capture frame set to 1000000000). At 2160x2160 and a 120 Hz preference, sustained gameplay fell to roughly 33-37 display/emulation FPS with about 27-30 ms GPU time. At 1080x1080 with the mirror enabled, gameplay held 60 emulation FPS but display cadence settled near 60 FPS. With 1080x1080, the mirror disabled and SteamVR set to 72 Hz, eight five-second gameplay intervals measured 71.9-72.2 display FPS and 59.8-60.2 emulation FPS. These are opening-gameplay samples rather than a full-game performance survey. The no-capture runs intentionally reached the diagnostic timeout; all recorded zero XR/OpenGL errors.

The runtime's `predictedDisplayPeriod` sometimes continued to report 120 Hz during the 72 Hz run, while the compositor logged its 72 Hz mode selection and the app submitted about 72 frames per second. The timing log therefore labels this field `runtimePeriod`, separately from measured display and emulation cadence.

The rebuilt default profile was then launched with the original configuration restored: it selected 1080x1080 eyes, 72 Hz and no mirror without test-setting overrides. It captured game frame 4000 with 281 models, submitted 4797 stereo projection frames and exited successfully with zero XR/OpenGL errors. Before the final capture, gameplay samples remained near 72 display FPS and 60 emulation FPS.

On Mesa/Zink, querying freshly generated GPU timer-query names produced four startup `GL_INVALID_OPERATION` errors. The timer now polls only slots containing a previously ended query and consumes each available result before reuse. The same diagnostic runs with `MESA_DEBUG=context` confirmed the startup errors were eliminated.

No new menu layout is introduced. The existing upstream PC input and display behavior remains available outside the Frame build.
