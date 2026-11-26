#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera();

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const;
    glm::vec3 forward() const;
    float fieldOfView() const;
    void move(const glm::vec3& localDirection, float dt);
    void look(float dx, float dy);
    void zoom(float amount);
    void reset();

private:
    glm::vec3 position_{0.0f};
    float yaw_{-130.0f};
    float pitch_{-18.0f};
    float fieldOfView_{60.0f};
};
