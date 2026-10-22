Windows build of Supermodel with the OpenXR VR mode, set up for **The Lost World: Jurassic Park**.

**Download** `LostWorldVR-windows.zip` below and unzip it. Put your own `lostwsga.zip` ROM set in the `ROMs` folder. Connect the headset, with Virtual Desktop, Quest Link or SteamVR as the active OpenXR runtime, and run `Play The Lost World VR.bat`. Full instructions are in `README-VR.txt` inside the zip and in the README. No ROMs or other game files are included.

## v0.1.0 — first release

- **Stereo 3D with head tracking**, the world drawn beyond the original 4:3 screen and the sky carried on past its edges.
- **Aim anywhere in view**, up to a screen-width past each edge of the original screen, with a red ring where your gun hand points (the game has no crosshair during play). The game's own cutscene sight is hidden, so there is only one.
- **Gun in your hand** from your own model: *Game* (`Assets/VRGameGun.obj`) in blue or red, chosen in the menu, or *Custom* (`Assets/VRGunModel.obj`). No model is included.
- **Recoil vibration**, right or left gun hand.
- **Steady 60 fps** whatever the headset's refresh rate.
- **Cinema mode** with a screen that keeps its size when the game zooms, TV-cabinet bezel, screen glow, 3D depth and the aiming ring on the screen.
- **No cutscene letterbox bars**; hit static and full-screen fades fill the whole view.
- **Options menu** in the headset, in the colours of the game's InGen screens.
- **Fast start**: after the first run, the game opens at the start of its attract demo.

Tested on a Meta Quest 3 with Virtual Desktop at 90 Hz.
