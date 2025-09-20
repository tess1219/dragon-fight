# Dragon Fight

A side-scrolling beat 'em up game inspired by classic titles like Double Dragon. Players control one or two fighters battling through stages filled with enemies, using punches, kicks, and jumps to progress.

## Built With

This project was developed collaboratively with **Grok-4-Fast-Reasoning**, an AI model powered by xAI. The AI handled code generation, debugging, asset integration, and optimizations based on user prompts, ensuring a streamlined C codebase using the raylib library for graphics, input, and audio.

- **Language**: C (GNU style)
- **Graphics/Input/Audio**: raylib (bundled in repo for self-contained builds)
- **Build System**: CMake
- **Assets**: Custom sprites and tilesets in `assets/` (inspired by retro beat 'em ups)

## Features

- Single-player with optional second player (activated by input).
- Multiple stages with parallax backgrounds, enemies, and level progression.
- Combat system: Jab, punch, kick attacks; health bars; death animations.
- UI: Stage info, enemy count, FPS counter, pause/restart.
- Camera follows lead player with smooth interpolation.

## System Requirements

- Linux (WSL2 recommended), Windows, or macOS.
- CMake 3.10+, Make (or Ninja), GCC/Clang.
- For WSL2: An X11 server like VcXsrv running on Windows.
- raylib source bundled in `raylib/` (cloned into project; no separate install).

## Build and Run Instructions

### On Linux/WSL2

1. **Set up DISPLAY for GUI (WSL2 only)**:  
   Ensure VcXsrv (or similar X11 server) is running on Windows with "Multiple windows" and "Disable access control" enabled. Then, in your WSL2 terminal:  
   ```bash
   export DISPLAY=:0
   ```  
   (Add to `~/.bashrc` for persistence: `echo 'export DISPLAY=:0' >> ~/.bashrc && source ~/.bashrc`.)  
   *Alternative if issues*: `export DISPLAY=$(awk '/nameserver / {print $2; exit}' /etc/resolv.conf):0.0`

2. **Clone/Navigate to Project** (includes bundled raylib):  
   ```bash
   git clone https://github.com/yourusername/dragon-fight.git  # Your repo URL; use --recurse-submodules if using submodule
   cd dragon-fight
   ```

3. **Build**:  
   Use a separate build directory to keep the source clean (raylib/ is built automatically):  
   ```bash
   rm -rf build  # Clean old build if needed
   mkdir build && cd build
   cmake ..  # Builds bundled raylib from raylib/ subdirectory
   make -j$(nproc)  # Parallel build using all CPU cores
   ```  
   This generates the `dragon-fight` executable using the git-cloned raylib source.

4. **Run the Game**:  
   From the `build/` directory:  
   ```bash
   ./dragon-fight
   ```  
   A window should open (800x600). Press ENTER to start.

### On Native Linux/Windows/macOS

Skip the DISPLAY step. Follow steps 2-4 above. For Windows, use MinGW/MSYS2 or Visual Studio with CMake. Ensure the clone includes `raylib/` for self-contained build.

### Troubleshooting

- **No window/display error**: Verify X11 server (VcXsrv) is running and DISPLAY is set. Test with `xclock` or `xeyes`.
- **OpenGL errors**: Install Mesa: `sudo apt install libgl1-mesa-glx libglu1-mesa-dev`. Add `export LIBGL_ALWAYS_INDIRECT=1` before running.
- **raylib not found or build fails**: Ensure `raylib/` folder was cloned (re-clone with `--recurse-submodules` if using submodule). Verify `raylib/CMakeLists.txt` exists. Check CMake output for errors during subdirectory build.
- **Large repo clone slow**: Use `git clone --depth 1` for a shallow clone (still includes raylib/ files).

## Controls

- **Player 1**: A/D (move), W (jump), J (jab), L (punch), K (kick)
- **Player 2** (if activated): Arrow keys (move), Up (jump), Z (jab), X (punch), C (kick)
- **Global**: P (pause), R (restart from game over/win), ESC (quit)

Player 2 joins automatically on second-player input and deactivates after 5s inactivity.

## Project Structure

- `main.c`: Game loop, UI, camera, state management.
- `assets.c/h`: Texture loading (backgrounds, sprites, tilesets).
- `entities.c/h`: Player and enemy logic (movement, combat, animations).
- `level.c/h`: Stage loading, collision, enemy spawning.
- `assets/`: PNG sprites, tilesets, and stage layers.
- `raylib/`: Bundled raylib source (git-cloned for self-contained builds; see CMakeLists.txt).
- `CMakeLists.txt`: Build configuration using local raylib (no system install).
- `build/`: Generated binaries and intermediates (ignored in .gitignore).

## Development Notes

- No backwards compatibility enforced; updates streamline the codebase.
- Assets are in `@kenny/` style directories but integrated directly.
- For web build: Use `build-tetsuo-web.sh` (if available).
- Contributions: Update existing files only; no new suffixes like `_enhanced`.

## License

MIT License (or as per raylib). Feel free to fork and modify!

---
*Generated with help from Grok-4-Fast-Reasoning on September 20, 2025.*
