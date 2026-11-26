#include "physics/physics_world.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float fixedTimeStep = 1.0f / 120.0f;
constexpr float maximumAccumulatedTime = 0.1f;
constexpr float contactSlop = 0.001f;
constexpr int solverIterations = 4;

bool finite_vec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float safe_clamp(float value, float fallback, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

glm::vec3 safe_vec3(const glm::vec3& value) {
    return finite_vec3(value) ? value : glm::vec3(0.0f);
}

float inverse_mass(const RigidBody& body) {
    return body.sleeping ? 0.0f : 1.0f / body.mass;
}

struct Contact {
    glm::vec3 normal{0.0f};
    float penetration{0.0f};
};

bool sphere_sphere_contact(const RigidBody& a, const RigidBody& b, Contact& contact) {
    const glm::vec3 delta = b.position - a.position;
    const float distanceSquared = glm::dot(delta, delta);
    const float radiusSum = a.radius + b.radius;
    if (distanceSquared >= radiusSum * radiusSum) {
        return false;
    }

    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
    contact.normal = distance > 1e-5f ? delta / distance : glm::vec3(1.0f, 0.0f, 0.0f);
    contact.penetration = radiusSum - distance;
    return true;
}

bool box_box_contact(const RigidBody& a, const RigidBody& b, Contact& contact) {
    const glm::vec3 delta = b.position - a.position;
    const glm::vec3 overlap = a.halfExtents + b.halfExtents - glm::abs(delta);
    if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
        return false;
    }

    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        contact.normal = {delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        contact.penetration = overlap.x;
    } else if (overlap.y <= overlap.z) {
        contact.normal = {0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
        contact.penetration = overlap.y;
    } else {
        contact.normal = {0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f};
        contact.penetration = overlap.z;
    }
    return true;
}

bool sphere_box_contact(const RigidBody& sphere, const RigidBody& box, Contact& contact) {
    const glm::vec3 lower = box.position - box.halfExtents;
    const glm::vec3 upper = box.position + box.halfExtents;
    const glm::vec3 closest = glm::clamp(sphere.position, lower, upper);
    const glm::vec3 delta = closest - sphere.position;
    const float distanceSquared = glm::dot(delta, delta);

    if (distanceSquared > 1e-8f) {
        const float distance = std::sqrt(distanceSquared);
        if (distance >= sphere.radius) {
            return false;
        }
        contact.normal = delta / distance;
        contact.penetration = sphere.radius - distance;
        return true;
    }

    const glm::vec3 toFace = box.halfExtents - glm::abs(sphere.position - box.position);
    if (toFace.x <= toFace.y && toFace.x <= toFace.z) {
        contact.normal = {sphere.position.x >= box.position.x ? -1.0f : 1.0f, 0.0f, 0.0f};
        contact.penetration = sphere.radius + toFace.x;
    } else if (toFace.y <= toFace.z) {
        contact.normal = {0.0f, sphere.position.y >= box.position.y ? -1.0f : 1.0f, 0.0f};
        contact.penetration = sphere.radius + toFace.y;
    } else {
        contact.normal = {0.0f, 0.0f, sphere.position.z >= box.position.z ? -1.0f : 1.0f};
        contact.penetration = sphere.radius + toFace.z;
    }
    return true;
}

bool find_contact(const RigidBody& a, const RigidBody& b, Contact& contact) {
    if (a.shape == ShapeType::Sphere && b.shape == ShapeType::Sphere) {
        return sphere_sphere_contact(a, b, contact);
    }
    if (a.shape == ShapeType::Box && b.shape == ShapeType::Box) {
        return box_box_contact(a, b, contact);
    }
    if (a.shape == ShapeType::Sphere) {
        return sphere_box_contact(a, b, contact);
    }

    const bool touching = sphere_box_contact(b, a, contact);
    contact.normal = -contact.normal;
    return touching;
}

void resolve_pair(RigidBody& a, RigidBody& b, const Contact& contact) {
    if (contact.penetration <= contactSlop) {
        return;
    }
    if (a.sleeping && !b.sleeping) {
        a.sleeping = false;
    }
    if (b.sleeping && !a.sleeping) {
        b.sleeping = false;
    }

    const float inverseMassA = inverse_mass(a);
    const float inverseMassB = inverse_mass(b);
    const float inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassSum == 0.0f) {
        return;
    }

    glm::vec3 relativeVelocity = b.velocity - a.velocity;
    const float velocityAlongNormal = glm::dot(relativeVelocity, contact.normal);
    float normalImpulseMagnitude = 0.0f;
    if (velocityAlongNormal < 0.0f) {
        const float restitution = velocityAlongNormal > -1.0f
                                      ? 0.0f
                                      : std::min(a.restitution, b.restitution);
        normalImpulseMagnitude =
            -(1.0f + restitution) * velocityAlongNormal / inverseMassSum;
        const glm::vec3 impulse = normalImpulseMagnitude * contact.normal;
        a.velocity -= impulse * inverseMassA;
        b.velocity += impulse * inverseMassB;

        relativeVelocity = b.velocity - a.velocity;
        glm::vec3 tangent = relativeVelocity -
                            glm::dot(relativeVelocity, contact.normal) * contact.normal;
        const float tangentLength = glm::length(tangent);
        if (tangentLength > 1e-5f) {
            tangent /= tangentLength;
            float tangentImpulse = -glm::dot(relativeVelocity, tangent) / inverseMassSum;
            const float friction = std::sqrt(a.friction * b.friction);
            tangentImpulse = std::clamp(tangentImpulse,
                                        -normalImpulseMagnitude * friction,
                                        normalImpulseMagnitude * friction);
            const glm::vec3 frictionImpulse = tangentImpulse * tangent;
            a.velocity -= frictionImpulse * inverseMassA;
            b.velocity += frictionImpulse * inverseMassB;
        }
    }

    const float correctionMagnitude =
        std::max(contact.penetration - contactSlop, 0.0f) * 0.8f / inverseMassSum;
    const glm::vec3 correction = correctionMagnitude * contact.normal;
    a.position -= correction * inverseMassA;
    b.position += correction * inverseMassB;
}

float ground_penetration(const RigidBody& body) {
    if (body.shape == ShapeType::Sphere) {
        return body.radius - body.position.y;
    }
    return body.halfExtents.y - body.position.y;
}

void resolve_ground(RigidBody& body) {
    const float penetration = ground_penetration(body);
    if (penetration <= 0.0f) {
        return;
    }

    body.position.y += std::max(penetration - contactSlop, 0.0f) * 0.8f;
    const float downwardSpeed = body.velocity.y;
    float normalImpulseMagnitude = 0.0f;
    if (downwardSpeed < 0.0f) {
        const float restitution = downwardSpeed > -1.0f ? 0.0f : body.restitution;
        normalImpulseMagnitude = -(1.0f + restitution) * downwardSpeed * body.mass;
        body.velocity.y += normalImpulseMagnitude / body.mass;

        glm::vec3 tangent{body.velocity.x, 0.0f, body.velocity.z};
        const float tangentLength = glm::length(tangent);
        if (tangentLength > 1e-5f) {
            const float frictionImpulse =
                std::min(tangentLength * body.mass, normalImpulseMagnitude * body.friction);
            tangent *= -frictionImpulse / tangentLength / body.mass;
            body.velocity.x += tangent.x;
            body.velocity.z += tangent.z;
        }
    }

    if (ground_penetration(body) <= 0.002f &&
        glm::dot(body.velocity, body.velocity) < 0.0025f) {
        body.velocity = glm::vec3(0.0f);
        body.sleeping = true;
    }
}

} // namespace

std::size_t PhysicsWorld::addBody(const RigidBody& body) {
    RigidBody safeBody = body;
    safeBody.position = safe_vec3(safeBody.position);
    safeBody.velocity = safe_vec3(safeBody.velocity);
    safeBody.radius = safe_clamp(safeBody.radius, 0.5f, 0.05f, 20.0f);
    safeBody.halfExtents = safe_vec3(safeBody.halfExtents);
    safeBody.halfExtents = glm::clamp(safeBody.halfExtents, glm::vec3(0.05f), glm::vec3(20.0f));
    safeBody.mass = safe_clamp(safeBody.mass, 1.0f, 0.05f, 1000.0f);
    safeBody.restitution = safe_clamp(safeBody.restitution, 0.55f, 0.0f, 1.0f);
    safeBody.friction = safe_clamp(safeBody.friction, 0.55f, 0.0f, 1.0f);
    safeBody.sleeping = false;
    bodies_.push_back(safeBody);
    return bodies_.size() - 1;
}

void PhysicsWorld::clear() {
    bodies_.clear();
    accumulator_ = 0.0f;
}

void PhysicsWorld::step(float dt) {
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    accumulator_ = std::min(accumulator_ + dt, maximumAccumulatedTime);
    int steps = 0;
    while (accumulator_ >= fixedTimeStep && steps < 12) {
        for (RigidBody& body : bodies_) {
            if (!body.sleeping) {
                body.velocity += gravity_ * fixedTimeStep;
                body.position += body.velocity * fixedTimeStep;
            }
        }

        for (int iteration = 0; iteration < solverIterations; ++iteration) {
            for (RigidBody& body : bodies_) {
                if (!body.sleeping) {
                    resolve_ground(body);
                }
            }

            for (std::size_t i = 0; i < bodies_.size(); ++i) {
                for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
                    Contact contact;
                    if (find_contact(bodies_[i], bodies_[j], contact)) {
                        resolve_pair(bodies_[i], bodies_[j], contact);
                    }
                }
            }
        }

        accumulator_ -= fixedTimeStep;
        ++steps;
    }
}

const std::vector<RigidBody>& PhysicsWorld::bodies() const {
    return bodies_;
}

void PhysicsWorld::setGravity(const glm::vec3& gravity) {
    constexpr float maximumGravityMagnitude = 100.0f;
    gravity_ = glm::clamp(safe_vec3(gravity),
                          glm::vec3(-maximumGravityMagnitude),
                          glm::vec3(maximumGravityMagnitude));
    const float magnitude = glm::length(gravity_);
    if (magnitude > maximumGravityMagnitude) {
        gravity_ *= maximumGravityMagnitude / magnitude;
    }
    for (RigidBody& body : bodies_) {
        body.sleeping = false;
    }
}
