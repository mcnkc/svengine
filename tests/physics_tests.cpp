#include "physics/physics_world.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_add_body_and_clear() {
    PhysicsWorld world;
    RigidBody body{};
    body.shape = ShapeType::Sphere;
    body.position = {0.0f, 4.0f, 0.0f};
    body.radius = 1.0f;

    check(world.addBody(body) == 0, "first body receives index zero");
    check(world.bodies().size() == 1, "body is stored in the world");

    world.clear();
    check(world.bodies().empty(), "clear removes every body");
}

RigidBody sphere_at(const glm::vec3& position, float radius = 0.5f) {
    RigidBody body{};
    body.shape = ShapeType::Sphere;
    body.position = position;
    body.radius = radius;
    return body;
}

RigidBody box_at(const glm::vec3& position, const glm::vec3& halfExtents) {
    RigidBody body{};
    body.shape = ShapeType::Box;
    body.position = position;
    body.halfExtents = halfExtents;
    return body;
}

void step_for(PhysicsWorld& world, int steps) {
    constexpr float fixedStep = 1.0f / 120.0f;
    for (int i = 0; i < steps; ++i) {
        world.step(fixedStep);
    }
}

void test_sphere_and_box_settle_on_ground() {
    PhysicsWorld world;
    world.addBody(sphere_at({-1.0f, 4.0f, 0.0f}));
    world.addBody(box_at({1.0f, 4.0f, 0.0f}, {0.5f, 0.75f, 0.5f}));

    step_for(world, 720);

    const auto& bodies = world.bodies();
    check(std::abs(bodies[0].position.y - 0.5f) < 0.03f,
          "falling sphere settles at its radius above the ground");
    check(std::abs(bodies[1].position.y - 0.75f) < 0.03f,
          "falling box settles at its half-height above the ground");
}

void test_body_pair_contacts_separate_all_shape_combinations() {
    {
        PhysicsWorld world;
        world.addBody(sphere_at({0.0f, 3.0f, 0.0f}));
        world.addBody(sphere_at({0.75f, 3.0f, 0.0f}));
        step_for(world, 1);
        check(glm::distance(world.bodies()[0].position, world.bodies()[1].position) >= 0.99f,
              "sphere-sphere contacts separate overlapping spheres");
    }
    {
        PhysicsWorld world;
        world.addBody(sphere_at({0.85f, 3.0f, 0.0f}));
        world.addBody(box_at({0.0f, 3.0f, 0.0f}, {0.5f, 0.5f, 0.5f}));
        step_for(world, 1);
        const auto& bodies = world.bodies();
        const glm::vec3 lower = bodies[1].position - bodies[1].halfExtents;
        const glm::vec3 upper = bodies[1].position + bodies[1].halfExtents;
        const glm::vec3 closest = glm::clamp(bodies[0].position, lower, upper);
        check(glm::distance(bodies[0].position, closest) >= 0.49f,
              "sphere-box contacts separate overlapping shapes");
    }
    {
        PhysicsWorld world;
        world.addBody(box_at({0.0f, 3.0f, 0.0f}, {0.5f, 0.5f, 0.5f}));
        world.addBody(box_at({0.75f, 3.0f, 0.0f}, {0.5f, 0.5f, 0.5f}));
        step_for(world, 1);
        check(std::abs(world.bodies()[1].position.x - world.bodies()[0].position.x) >= 0.99f,
              "box-box contacts separate overlapping boxes");
    }
}

void test_restitution_bounces_and_friction_reduces_sliding() {
    PhysicsWorld bounceWorld;
    RigidBody bouncy = sphere_at({0.0f, 1.5f, 0.0f});
    bouncy.restitution = 0.9f;
    bounceWorld.addBody(bouncy);
    step_for(bounceWorld, 80);
    check(bounceWorld.bodies()[0].velocity.y > 0.5f,
          "restitution produces an upward bounce after ground impact");

    PhysicsWorld frictionWorld;
    RigidBody slider = sphere_at({0.0f, 1.0f, 0.0f});
    slider.velocity = {3.0f, 0.0f, 0.0f};
    slider.restitution = 0.0f;
    slider.friction = 1.0f;
    frictionWorld.addBody(slider);
    step_for(frictionWorld, 240);
    check(std::abs(frictionWorld.bodies()[0].velocity.x) < 0.5f,
          "ground friction reduces horizontal sliding speed");
}

void test_invalid_material_properties_are_clamped() {
    PhysicsWorld world;
    RigidBody body = sphere_at({0.0f, 2.0f, 0.0f});
    body.mass = -1.0f;
    body.restitution = 3.0f;
    body.friction = -2.0f;
    world.addBody(body);

    const auto& added = world.bodies()[0];
    check(added.mass > 0.0f, "mass is clamped to a positive value");
    check(added.restitution >= 0.0f && added.restitution <= 1.0f,
          "restitution is clamped to the unit interval");
    check(added.friction >= 0.0f && added.friction <= 1.0f,
          "friction is clamped to the unit interval");

    body.mass = std::numeric_limits<float>::quiet_NaN();
    world.addBody(body);
    check(std::isfinite(world.bodies()[1].mass) && world.bodies()[1].mass > 0.0f,
          "non-finite mass is replaced with a safe positive value");
}

void test_large_frame_gap_does_not_leave_simulation_backlog() {
    PhysicsWorld delayedWorld;
    PhysicsWorld boundedWorld;
    delayedWorld.addBody(sphere_at({0.0f, 100.0f, 0.0f}));
    boundedWorld.addBody(sphere_at({0.0f, 100.0f, 0.0f}));

    constexpr float fixedStep = 1.0f / 120.0f;
    delayedWorld.step(10.0f);
    delayedWorld.step(fixedStep);
    boundedWorld.step(0.1f);
    boundedWorld.step(fixedStep);

    check(glm::distance(delayedWorld.bodies()[0].position,
                        boundedWorld.bodies()[0].position) < 0.001f,
          "large frame gaps are capped without accumulating future work");
    check(glm::distance(delayedWorld.bodies()[0].velocity,
                        boundedWorld.bodies()[0].velocity) < 0.001f,
          "large frame gaps preserve the same bounded-step velocity");
}

void test_gravity_magnitude_is_bounded() {
    PhysicsWorld world;
    world.addBody(sphere_at({0.0f, 2.0f, 0.0f}));
    world.setGravity({1.0e20f, -1.0e20f, 1.0e20f});
    world.step(1.0f / 120.0f);

    check(glm::length(world.bodies()[0].velocity) <= 0.84f,
          "extreme finite gravity is bounded before integration");
}

} // namespace

int main() {
    test_add_body_and_clear();
    test_sphere_and_box_settle_on_ground();
    test_body_pair_contacts_separate_all_shape_combinations();
    test_restitution_bounces_and_friction_reduces_sliding();
    test_invalid_material_properties_are_clamped();
    test_large_frame_gap_does_not_leave_simulation_backlog();
    test_gravity_magnitude_is_bounded();
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All physics tests passed\n";
    return EXIT_SUCCESS;
}
