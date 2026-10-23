# Steam Frame package

`lost-world-supermodel-frame-linux-arm64.tar.gz` is a portable native ARM64 Linux build. Extract it to a writable or read-only installation directory and launch `run-frame.sh` from Steam or a terminal. The launcher stores configuration, assets and ROMs under `$XDG_DATA_HOME/lost-world-supermodel-frame` (or `$HOME/.local/share/lost-world-supermodel-frame`) and copies packaged defaults only when a file is absent. Supermodel creates its default `Config/Supermodel.ini` there on first launch; a personal configuration is never replaced.

The package contains the emulator, bundled SDL2_net, OpenXR loader and jsoncpp dependencies, their upstream copyright/license notices, public game/music databases, the crosshair assets and the licensed menu font. It contains no ROM, save/NVRAM, local `Supermodel.ini`, private gun model or menu artwork. Add your own `ROMs/lostwsga.zip` under the persistent data directory, or pass a ROM path to the launcher.

Examples:

```sh
./run-frame.sh --probe 30
./run-frame.sh --probe 30 --force-render
./run-frame.sh --flat /path/to/lostwsga.zip -show-fps
./run-frame.sh /path/to/lostwsga.zip
```

The force-render switch is diagnostic and applies only to the ROM-free probe. The probe reports the runtime's original `shouldRender` state and exits unsuccessfully if it submits no stereo projection, sees an OpenXR/OpenGL error, or samples an unexpected eye colour. `--flat` passes the Frame-only `-no-vr` override so a saved `VR=true` cannot start an XR session.

Build reproduction and current renderer/frame-gate notes are in `VR_ARCHITECTURE.md`. `BUILD-METADATA.json` records source/tree and toolchain identities; `SHA256SUMS` covers each shipped file.
