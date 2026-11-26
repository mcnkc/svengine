#pragma once

#include "app/camera.hpp"
#include "physics/physics_world.hpp"

#include <glm/glm.hpp>

#include <memory>

struct HudState {
    bool paused{false};
    int bodyCount{0};
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    ShapeType spawnShape{ShapeType::Sphere};
    float mass{1.0f};
    float restitution{0.55f};
    float friction{0.55f};
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(int width, int height);
    void resize(int width, int height);
    void draw(const PhysicsWorld& world, const Camera& camera, const HudState& hud);
    void shutdown();

private:
    struct State;
    std::unique_ptr<State> state_;
};
