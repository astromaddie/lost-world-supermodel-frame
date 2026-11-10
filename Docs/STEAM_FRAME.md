# Steam Frame Package

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

On Frame, the eye size is the absolute `VRMaxEyeWidth` target (2160 by default) multiplied by `VRResolutionScale` in each dimension, capped by the runtime's maximum image size. Thus a 2160 target with scale 0.5 renders 1080x1080 per eye, with one quarter of the pixels of scale 1.0. The 3D renderer draws directly at that eye size; the flat window's `-res=992,768` does not limit the eye buffers. Original arcade textures and 2D layers retain their own source detail. Check the startup log's `eyes WxH` field for the actual allocated resolution.

Build reproduction and current renderer/frame-gate notes are in `VR_ARCHITECTURE.md`. `BUILD-METADATA.json` records source/tree and toolchain identities; `SHA256SUMS` covers each shipped file.

## Building from Source

Run these commands from a fresh checkout of this fork's `steam-frame` branch:

```sh
git clone -b steam-frame https://github.com/astromaddie/lost-world-supermodel-frame.git
cd lost-world-supermodel-frame
./Scripts/frame/build.sh
```

You need Git, a POSIX shell and a running Docker engine with Linux ARM64 container
support. The script passes `--platform linux/arm64` for both build and run. Apple
Silicon macOS and ARM64 Linux are suitable hosts. On x86-64 Linux, Intel macOS or
Windows with WSL2, the Docker engine also needs ARM64 emulation; the script doesn't
install or configure it. Use the shell inside WSL for the commands on Windows.
The target is always Linux ARM64, never a macOS app or Windows executable.

The first run needs network access to Docker Hub and Ubuntu's package archives.
`Scripts/frame/Dockerfile` pins the Ubuntu 22.04 image digest and package versions,
including GCC 11, SDL2, SDL2_net, OpenGL/GLU, X11, zlib and the OpenXR loader.
There are no submodules and no game data is needed to compile or verify the package.
If Docker reports that a pinned package version is unavailable, check the Dockerfile
against the Jammy archive rather than silently switching to a newer distro: the
packager checks the Frame ABI floor.

`build.sh` performs these steps in order:

1. Build the `lost-world-frame-build:local` Docker image.
2. Clean and compile `Makefiles/Makefile.Frame` with two jobs. Objects go in
   `objframe/` and the executable is `binframe/supermodel`.
3. Run `Scripts/frame/package.sh`. It checks AArch64 ELF headers, library
   dependencies and GLIBC/GLIBCXX versions, and writes a ROM-free package with
   `BUILD-METADATA.json` and `SHA256SUMS`.
4. Run `Scripts/frame/verify.sh`: command-line validation, launcher and ABI tests,
   a missing-runtime failure check, input/aim tests and OpenGL renderer regressions
   under Xvfb. These are host checks, not a SteamVR session.

The outputs are `dist/lost-world-supermodel-frame-linux-arm64/` and the matching
`.tar.gz`. Copy the archive to the headset, extract it, and create the Steam shortcut
described above. Don't add ROMs or personal assets to the generated package folder:
repackaging verifies the old checksums before replacing it and refuses unrecognized
or modified output. Put those files in the launcher's persistent data directory.

To check an extracted package on Linux, run `sha256sum -c SHA256SUMS` from inside it.
On macOS use `shasum -a 256 -c SHA256SUMS`. For a ROM-free headset check, launch
`run-frame.sh --probe 30` through Steam. `--force-render` is only for deliberate
standby diagnostics; ordinary gameplay doesn't use it.
