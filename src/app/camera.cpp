#include "app/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float degreesToRadians = 0.01745329251994329577f;
constexpr float movementSpeed = 6.0f;
constexpr float mouseSensitivity = 0.1f;
constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};

bool finite_vec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

Camera::Camera() {
    reset();
}

glm::mat4 Camera::viewMatrix() const {
    const glm::vec3 direction = forward();
    return glm::lookAt(position_, position_ + direction, worldUp);
}

glm::vec3 Camera::position() const {
    return position_;
}

glm::vec3 Camera::forward() const {
    const float yawRadians = yaw_ * degreesToRadians;
    const float pitchRadians = pitch_ * degreesToRadians;
    return glm::normalize(glm::vec3(std::cos(yawRadians) * std::cos(pitchRadians),
                                    std::sin(pitchRadians),
                                    std::sin(yawRadians) * std::cos(pitchRadians)));
}

float Camera::fieldOfView() const {
    return fieldOfView_;
}

void Camera::move(const glm::vec3& localDirection, float dt) {
    if (!finite_vec3(localDirection) || !std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    glm::vec3 direction = forward() * localDirection.z +
                          glm::normalize(glm::cross(forward(), worldUp)) * localDirection.x +
                          worldUp * localDirection.y;
    const float directionLength = glm::length(direction);
    if (directionLength > 1.0f) {
        direction /= directionLength;
    }
    position_ += direction * movementSpeed * dt;
}

void Camera::look(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) {
        return;
    }
    yaw_ += dx * mouseSensitivity;
    pitch_ = std::clamp(pitch_ - dy * mouseSensitivity, -89.0f, 89.0f);
}

void Camera::zoom(float amount) {
    if (!std::isfinite(amount)) {
        return;
    }
    fieldOfView_ = std::clamp(fieldOfView_ - amount, 25.0f, 75.0f);
}

void Camera::reset() {
    position_ = {10.0f, 8.0f, 12.0f};
    yaw_ = -130.0f;
    pitch_ = -18.0f;
    fieldOfView_ = 60.0f;
}
