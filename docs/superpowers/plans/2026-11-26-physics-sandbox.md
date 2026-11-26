# 3D Physics Sandbox Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native interactive C++ physics sandbox with a visible 3D scene, responsive controls, and understandable rigid-body behavior.

**Architecture:** Keep physics in a standalone library with deterministic fixed-step simulation and tests. The GLFW application owns input, camera, rendering, HUD, and user-facing defaults; OpenGL handles lit meshes and directional shadows.

**Tech Stack:** C++20, CMake, OpenGL 3.3, GLFW, GLM, CTest.

**Spec:** `docs/superpowers/specs/2026-11-26-physics-sandbox-design.md`

## Global Constraints

- C++ with OpenGL rendering, GLFW window/input, and GLM math.
- Keep physics independent of graphics and use a fixed simulation timestep.
- Keep dependencies lightweight; provide clear CMake build/run instructions.
- Reset clears all spawned objects, restores default simulation and spawn settings, and returns the camera to its initial view.

## Review Focus

- Zero, negative, or extreme mass/property inputs must be clamped/rejected rather than destabilizing the solver.
- Large frame-time gaps must not cause unbounded catch-up work or tunneling-like behavior.
- Camera input at window edges and zero/near-zero view directions must not produce invalid math.
- Missing shader/context/FBO support must fail visibly with a useful diagnostic.
- Spawn/reset/pause actions must remain safe while the simulation has zero or many bodies.

---

## File Structure

- `CMakeLists.txt`: dependency discovery, app target, physics library, tests.
- `include/physics/physics_world.hpp`, `src/physics/physics_world.cpp`: body state, fixed-step stepping, gravity, contacts, impulses, friction, settling.
- `tests/physics_tests.cpp`: focused deterministic tests for ground and pair contacts, configurable response, and invalid inputs.
- `include/render/renderer.hpp`, `src/render/renderer.cpp`: OpenGL resource setup, meshes, shaders, shadows, grid, HUD.
- `include/app/camera.hpp`, `src/app/camera.cpp`: free camera state and movement.
- `src/main.cpp`: GLFW lifecycle, callbacks, controls, simulation loop, app configuration.
- `README.md`: dependencies, build/run commands, controls, and physics limitations.

### Task 1: Create Build Skeleton and Physics Interfaces

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/physics/physics_world.hpp`
- Create: `src/physics/physics_world.cpp`
- Create: `tests/physics_tests.cpp`

**Interfaces:**
- `ShapeType { Sphere, Box }`
- `RigidBody`: `shape`, `position`, `velocity`, `halfExtents`, `radius`, `mass`, `restitution`, `friction`, `sleeping`.
- `PhysicsWorld`: `addBody(const RigidBody&) -> std::size_t`, `clear() -> void`, `step(float dt) -> void`, `bodies() -> const std::vector<RigidBody>&`, `setGravity(const glm::vec3&) -> void`.
- Fixed step: `1.0f / 120.0f`; maximum frame input accepted by the app: `0.1s`.

- [ ] **Step 1: Define only the CMake/CTest configuration and test target.**
- [ ] **Step 2: Write named tests for valid sphere insertion and world clearing against the planned public interface.**
- [ ] **Step 3: Run `cmake -S . -B build && cmake --build build --target physics_tests`; expected result is compilation failure because the physics interface is not implemented yet.**
- [ ] **Step 4: Implement the minimum public interface and storage in the physics module.**
- [ ] **Step 5: Rebuild and run `ctest --test-dir build --output-on-failure`; expected result is all baseline tests pass.**

### Task 2: Implement and Test the Physics Solver

**Files:**
- Modify: `src/physics/physics_world.cpp`
- Modify: `tests/physics_tests.cpp`

**Interfaces:**
- Consume Task 1 `RigidBody` and `PhysicsWorld` interfaces without rendering dependencies.
- Add `setSpawnProperties(float mass, float restitution, float friction) -> void` only if needed by app defaults; keep per-body properties on `RigidBody`.

- [ ] **Step 1: Add a ground-contact test asserting a falling sphere settles at radius height and a falling box settles at half-height.**
- [ ] **Step 2: Add sphere-sphere, sphere-box, and box-box overlap tests asserting separation and non-penetrating response.**
- [ ] **Step 3: Add restitution and friction tests using numerical tolerances; add invalid-mass/property tests asserting safe clamping.**
- [ ] **Step 4: Implement fixed-substep gravity/integration, contact generation, restitution impulses, Coulomb-style friction, position correction, and low-energy settling.**
- [ ] **Step 5: Run `ctest --test-dir build --output-on-failure`; expected result is all physics tests pass deterministically.**

### Task 3: Implement Camera and OpenGL Scene Rendering

**Files:**
- Create: `include/app/camera.hpp`
- Create: `src/app/camera.cpp`
- Create: `include/render/renderer.hpp`
- Create: `src/render/renderer.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `Camera`: `viewMatrix() -> glm::mat4`, `move(const glm::vec3&, float dt) -> void`, `look(float dx, float dy) -> void`, `zoom(float amount) -> void`, `reset() -> void`.
- `Renderer`: `initialize(int width, int height) -> bool`, `resize(int width, int height) -> void`, `draw(const PhysicsWorld&, const Camera&, const HudState&) -> void`, `shutdown() -> void`.
- `HudState`: pause state, body count, gravity, spawn shape, mass, restitution, and friction.

- [ ] **Step 1: Implement safe free-camera basis/view math and test camera reset, movement, and bounded zoom.**
- [ ] **Step 2: Add shader compilation with explicit error reporting, cube/sphere meshes, ground/grid, and visible sun.**
- [ ] **Step 3: Add directional depth-map shadows and lit scene rendering.**
- [ ] **Step 4: Add a compact text HUD for current values and controls; render only with initialized resources.**
- [ ] **Step 5: Build the application target; expected result is successful compilation and no OpenGL errors at initialization.**

### Task 4: Integrate Controls and Simulation Lifecycle

**Files:**
- Create: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- GLFW owns the window and callbacks; `PhysicsWorld` owns simulation state; `Renderer` consumes immutable body/camera/HUD state.
- Defaults: gravity `(0, -9.81, 0)`, mass `1kg`, restitution `0.55`, friction `0.55`.

- [ ] **Step 1: Add GLFW initialization, OpenGL context creation, error handling, and orderly shutdown.**
- [ ] **Step 2: Add a fixed-step accumulator capped to `0.1s` frame input and run physics at `1/120s` only while unpaused.**
- [ ] **Step 3: Implement Space pause, R reset, 1 sphere, 2 cube, WASD + vertical camera movement, right-mouse look, and scroll zoom.**
- [ ] **Step 4: Implement gravity and spawn-default adjustment keys; clamp values and show the exact controls and live parameters in HUD/title.**
- [ ] **Step 5: Verify startup, pause/resume, both spawn actions, gravity changes, and reset in a running graphical session.**

### Task 5: Document and Verify the Complete Application

**Files:**
- Create: `README.md`
- Modify: `CMakeLists.txt`, implementation files only if validation reveals defects.

- [ ] **Step 1: Document Linux build dependencies, configure/build/run commands, controls, and solver limitations.**
- [ ] **Step 2: Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`.**
- [ ] **Step 3: Launch the app when a display is available and verify a spawned sphere and cube fall, collide, bounce, and settle.**
- [ ] **Step 4: Confirm the final README commands match the tested build and run path.**
