#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

enum class ShapeType {
    Sphere,
    Box
};

struct RigidBody {
    ShapeType shape{ShapeType::Sphere};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 halfExtents{0.5f};
    float radius{0.5f};
    float mass{1.0f};
    float restitution{0.55f};
    float friction{0.55f};
    bool sleeping{false};
};

class PhysicsWorld {
public:
    std::size_t addBody(const RigidBody& body);
    void clear();
    void step(float dt);
    const std::vector<RigidBody>& bodies() const;
    void setGravity(const glm::vec3& gravity);

private:
    std::vector<RigidBody> bodies_;
    glm::vec3 gravity_{0.0f, -9.81f, 0.0f};
    float accumulator_{0.0f};
};
