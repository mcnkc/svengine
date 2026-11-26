# 3D Physics Sandbox

A small native C++ physics sandbox rendered with OpenGL 3.3. It includes a
fixed-step rigid-body simulation, a lit ground plane with a scale grid and
directional shadows, and a free camera. The initial scene is empty; use the
spawn controls to drop spheres and cubes into the world.

## Requirements

- C++20 compiler
- CMake 3.20 or newer
- OpenGL 3.3 development libraries
- GLFW 3.3 or newer
- GLM
- A desktop display server with OpenGL support

On Ubuntu or Debian, install the build dependencies with:

```sh
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libglm-dev libgl1-mesa-dev
```

## Build and run

From the project directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/physics_sandbox
```

## Controls

| Input | Action |
| --- | --- |
| `Space` | Pause or resume |
| `R` | Clear objects and restore simulation, spawn, and camera defaults |
| `1` / `2` | Spawn a sphere / cube above the ground |
| `[` / `]` | Make gravity weaker / stronger |
| `,` / `.` | Decrease / increase mass for newly spawned objects |
| `;` / `'` | Decrease / increase restitution for newly spawned objects |
| `N` / `M` | Decrease / increase friction for newly spawned objects |
| `W` / `A` / `S` / `D` | Move the camera |
| `E` / `Q` | Move the camera up / down |
| Hold right mouse button and move | Look around |
| Mouse wheel | Zoom |
| `Escape` | Close the application |

The on-screen HUD displays live simulation values and the available controls.
Physics uses a 1/120-second fixed step, gravity, impulse-based restitution and
friction, ground contacts, and approximate sphere-sphere, sphere-box, and
axis-aligned box-box contacts. It is intentionally a small educational solver:
boxes do not rotate, and it does not implement arbitrary meshes, joints, or a
full constraint solver.
