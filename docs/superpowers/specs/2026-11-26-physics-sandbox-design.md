# 3D Physics Sandbox Design

## Intent and goals

Create a native, interactive C++ application that makes gravity, collision,
bouncing, and settling easy to observe. It will render a ground plane, grid,
directional sunlight, and spawnable cubes and spheres. Users can pause/reset,
spawn objects, change simulation properties, and navigate with a free camera.

## Non-goals

This is a focused demo, not a general-purpose engine: no importing assets,
arbitrary mesh collision, networking, scripting, or full constraint solver.

## Constraints

- C++ with OpenGL rendering, GLFW window/input, and GLM math.
- Keep physics independent of graphics and use a fixed simulation timestep.
- Keep dependencies lightweight; provide clear CMake build/run instructions.

## Approaches

1. Use a physics library and a rendering/UI framework: quickest feature coverage,
   but adds dependencies and hides the core simulation the project is meant to
   demonstrate.
2. Implement a small physics solver and renderer from scratch: requires more
   code, but keeps behavior understandable and fulfills the "from scratch"
   requirement. This is the selected approach.
3. Use a browser-based/WebGL implementation: easier to distribute, but violates
   the requested C++/OpenGL/GLFW technology.

## Design

- CMake project with separate physics, rendering, camera/input, and application
  modules.
- Physics uses fixed-step semi-implicit Euler integration, gravity, sphere and
  axis-aligned box shapes, ground contacts, and approximate pairwise contacts.
  Mass, restitution, and friction are configurable; sleeping/settling behavior
  prevents endless tiny motion.
- Collision pairs cover sphere-sphere, sphere-box, and box-box, using the
  selected shapes' simple collision volumes rather than a full rigid-body
  constraint solver.
- OpenGL renders lit meshes, a directional shadow map, visible sun, ground, and
  scale grid.
- GLFW handles free-camera WASD/mouse/scroll and keyboard simulation controls.
  A compact in-window HUD and window title show state and available controls.
- Gravity is adjustable during play. Mass, restitution, and friction are
  adjustable spawn defaults; changing them affects newly spawned objects.
- Reset clears all spawned objects, restores default simulation and spawn
  settings, and returns the camera to its initial view. Startup shows the
  environment without objects so spawning is an explicit user action.
- The builder may choose exact keys, numeric limits, and low-level
  implementation details as long as they remain discoverable and consistent.

## Acceptance

The project builds and launches; users can spawn either shape above the ground,
observe gravity, collisions, bouncing, and settling, and control/reset the
simulation. README documents dependencies, build/run commands, and controls.
