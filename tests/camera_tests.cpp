#include "app/camera.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_camera_movement_and_reset() {
    Camera camera;
    const glm::vec3 initialPosition = camera.position();
    const glm::vec3 initialForward = camera.forward();

    camera.move({1.0f, 0.0f, 0.0f}, 1.0f);
    check(glm::distance(camera.position(), initialPosition) > 1.0f,
          "camera movement changes its position");

    camera.look(120.0f, -10000.0f);
    check(std::abs(glm::length(camera.forward()) - 1.0f) < 0.001f,
          "camera look keeps a normalized direction");
    camera.zoom(1000.0f);
    check(camera.fieldOfView() >= 25.0f && camera.fieldOfView() <= 75.0f,
          "camera zoom remains within its field-of-view limits");

    camera.reset();
    check(glm::distance(camera.position(), initialPosition) < 0.001f,
          "camera reset restores its initial position");
    check(glm::distance(camera.forward(), initialForward) < 0.001f,
          "camera reset restores its initial view direction");
    check(std::abs(camera.fieldOfView() - 60.0f) < 0.001f,
          "camera reset restores the default field of view");
}

} // namespace

int main() {
    test_camera_movement_and_reset();
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All camera tests passed\n";
    return EXIT_SUCCESS;
}
