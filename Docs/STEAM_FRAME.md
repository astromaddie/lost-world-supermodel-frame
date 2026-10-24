# Steam Frame package

`lost-world-supermodel-frame-linux-arm64.tar.gz` is a portable native ARM64 Linux build. Extract it to a writable or read-only installation directory and launch `run-frame.sh` from Steam or a terminal. The launcher stores configuration, assets and ROMs under `$XDG_DATA_HOME/lost-world-supermodel-frame` (or `$HOME/.local/share/lost-world-supermodel-frame`) and copies packaged defaults only when a file is absent. Supermodel creates its default `Config/Supermodel.ini` there on first launch; a personal configuration is never replaced.

Use a Steam shortcut with Steam Linux Runtime 4 ARM64 for tracked-controller play. Terminal launches are useful for the flat mode and diagnostics.

The package contains the emulator, bundled SDL2_net, OpenXR loader and jsoncpp dependencies, their upstream copyright/license notices, public game/music databases, the crosshair assets and the licensed menu font. It contains no ROM, save/NVRAM, local `Supermodel.ini`, private gun model or menu artwork. Add your own `ROMs/lostwsga.zip` under the persistent data directory, or pass a ROM path to the launcher.

The Game gun option includes the Namco Arcade Gun model by Martoscar under CC BY 4.0, with attribution and conversion details in `Assets/FrameArcadeGun-NOTICE.txt` and `FrameArcadeGun.json`. A personal `Assets/VRGameGun.obj` takes precedence, and the Custom option still uses your `VRGunModel.obj`. Lost World's matching wide-range aim calibration defaults are applied automatically even without the old PC configuration file; explicit calibration settings remain respected.

Examples:

```sh
./run-frame.sh --probe 30
./run-frame.sh --probe 30 --force-render
./run-frame.sh --flat /path/to/lostwsga.zip -show-fps
./run-frame.sh /path/to/lostwsga.zip
./run-frame.sh /path/to/lostwsga.zip -vr-game-diagnostic -vr-game-diagnostic-yaw=90
```

The force-render switch is diagnostic and applies only to the ROM-free probe. The probe reports the runtime's original `shouldRender` state and exits unsuccessfully if it submits no stereo projection, sees an OpenXR/OpenGL error, or samples an unexpected eye colour. `--flat` passes the Frame-only `-no-vr` override so a saved `VR=true` cannot start an XR session.

`-vr-game-diagnostic` is a separate bounded ROM-backed capture. It runs the game while SteamVR reports standby and saves left/right PPMs from the production final eye FBOs under `FrameDiagnostics/` in the persistent data directory. Its sidecar records the game frame and model count, predicted display time, original `shouldRender` and forced-render state, eye dimensions, submitted projection layer, and `xrEndFrame` result. Choose `-vr-game-diagnostic-yaw=0`, `90`, or `180` for fixed synthetic head and hand poses; the default is zero. Capture starts at game frame 1500 by default and the timeout is 120 seconds. It exits unsuccessfully if it cannot capture both gameplay eyes cleanly. `SM_FRAME_DIAG_SEQUENCE=8 SM_FRAME_DIAG_HOLD_SCENE=1` captures a retained scene with fixed head pose, asymmetric eye FOVs and IPD; the same locked views are rendered and submitted. Sidecars include both eye FOVs, current/source game frusta, source frame and coverage counts. Each `.coverage.pgm` is a byte mask read from New3D before compositing: bits 0/1 are nonzero/opaque base alpha, 2/3 and 4/5 the two transparency layers, 6 nonzero depth and 7 nonfinite depth. Depth belongs to the last rendered priority/overlay; colour coverage accumulates across priorities. Compare these masks with backdrop/final images to locate missing geometry versus compositing differences. Readbacks force GPU completion and change scheduling, so this diagnostic cannot establish ordinary presentation stability.

For the normal Frame profile, trigger fires, gun-hand squeeze reloads, right A/Y starts, right B/X inserts a coin, right Menu toggles the existing options/pause panel, and left View tap closes an open panel. Holding View for one second recentres once and pulses both controllers. Valve Index and Touch fallback bindings remain available; stick-click recentring remains on those fallback profiles.

The standalone defaults are 1080x1080 per eye (0.5x render scale), a 72 Hz request, and desktop mirroring off. The existing render-resolution option can raise the scale to 1.0x for 2160x2160 eyes on the next launch. `VRRefreshHz` and `VRMirror` remain configurable in `Config/Supermodel.ini`; SteamVR's per-application refresh preference determines which rates it offers.

Build reproduction and current renderer/frame-gate notes are in `VR_ARCHITECTURE.md`. `BUILD-METADATA.json` records source/tree and toolchain identities; `SHA256SUMS` covers each shipped file.
