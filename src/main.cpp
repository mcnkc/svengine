#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {

constexpr glm::vec3 defaultGravity{0.0f, -9.81f, 0.0f};
constexpr float defaultMass = 1.0f;
constexpr float defaultRestitution = 0.55f;
constexpr float defaultFriction = 0.55f;

struct Application {
    PhysicsWorld world;
    Camera camera;
    Renderer renderer;
    glm::vec3 gravity{defaultGravity};
    float spawnMass{defaultMass};
    float spawnRestitution{defaultRestitution};
    float spawnFriction{defaultFriction};
    ShapeType spawnShape{ShapeType::Sphere};
    bool paused{false};
    bool rightMouseDown{false};
    bool hasLastCursorPosition{false};
    double lastCursorX{0.0};
    double lastCursorY{0.0};
    std::size_t spawnSequence{0};
};

Application* application(GLFWwindow* window) {
    return static_cast<Application*>(glfwGetWindowUserPointer(window));
}

void spawn_body(Application& app, ShapeType shape) {
    if (app.world.bodies().size() >= 200) {
        return;
    }

    const std::size_t slot = app.spawnSequence++;
    const int column = static_cast<int>(slot % 5);
    const int row = static_cast<int>(slot / 5);
    RigidBody body;
    body.shape = shape;
    body.position = {static_cast<float>(column - 2) * 1.15f,
                     7.0f + static_cast<float>(row) * 1.25f,
                     static_cast<float>(row % 3) * 1.2f - 1.2f};
    body.mass = app.spawnMass;
    body.restitution = app.spawnRestitution;
    body.friction = app.spawnFriction;
    app.world.addBody(body);
    app.spawnShape = shape;
}

void reset_simulation(Application& app) {
    app.world.clear();
    app.gravity = defaultGravity;
    app.world.setGravity(app.gravity);
    app.spawnMass = defaultMass;
    app.spawnRestitution = defaultRestitution;
    app.spawnFriction = defaultFriction;
    app.spawnShape = ShapeType::Sphere;
    app.paused = false;
    app.spawnSequence = 0;
    app.camera.reset();
}

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (Application* app = application(window)) {
        app->renderer.resize(width, height);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int) {
    Application* app = application(window);
    if (app == nullptr || button != GLFW_MOUSE_BUTTON_RIGHT) {
        return;
    }

    app->rightMouseDown = action == GLFW_PRESS;
    app->hasLastCursorPosition = false;
    if (app->rightMouseDown) {
        glfwGetCursorPos(window, &app->lastCursorX, &app->lastCursorY);
        app->hasLastCursorPosition = true;
    }
}

void cursor_position_callback(GLFWwindow* window, double x, double y) {
    Application* app = application(window);
    if (app == nullptr || !app->rightMouseDown) {
        return;
    }
    if (app->hasLastCursorPosition) {
        app->camera.look(static_cast<float>(x - app->lastCursorX),
                         static_cast<float>(y - app->lastCursorY));
    }
    app->lastCursorX = x;
    app->lastCursorY = y;
    app->hasLastCursorPosition = true;
}

void scroll_callback(GLFWwindow* window, double, double yOffset) {
    if (Application* app = application(window)) {
        app->camera.zoom(static_cast<float>(yOffset) * 2.0f);
    }
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    Application* app = application(window);
    if (app == nullptr || action != GLFW_PRESS) {
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    case GLFW_KEY_SPACE:
        app->paused = !app->paused;
        break;
    case GLFW_KEY_R:
        reset_simulation(*app);
        break;
    case GLFW_KEY_1:
        spawn_body(*app, ShapeType::Sphere);
        break;
    case GLFW_KEY_2:
        spawn_body(*app, ShapeType::Box);
        break;
    case GLFW_KEY_LEFT_BRACKET:
        app->gravity.y = std::min(app->gravity.y + 0.5f, -0.5f);
        app->world.setGravity(app->gravity);
        break;
    case GLFW_KEY_RIGHT_BRACKET:
        app->gravity.y = std::max(app->gravity.y - 0.5f, -30.0f);
        app->world.setGravity(app->gravity);
        break;
    case GLFW_KEY_COMMA:
        app->spawnMass = std::max(0.1f, app->spawnMass - 0.1f);
        break;
    case GLFW_KEY_PERIOD:
        app->spawnMass = std::min(100.0f, app->spawnMass + 0.1f);
        break;
    case GLFW_KEY_SEMICOLON:
        app->spawnRestitution = std::max(0.0f, app->spawnRestitution - 0.05f);
        break;
    case GLFW_KEY_APOSTROPHE:
        app->spawnRestitution = std::min(1.0f, app->spawnRestitution + 0.05f);
        break;
    case GLFW_KEY_N:
        app->spawnFriction = std::max(0.0f, app->spawnFriction - 0.05f);
        break;
    case GLFW_KEY_M:
        app->spawnFriction = std::min(1.0f, app->spawnFriction + 0.05f);
        break;
    default:
        break;
    }
}

glm::vec3 read_camera_input(GLFWwindow* window) {
    const float horizontal =
        (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f) -
        (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0f : 0.0f);
    const float forward =
        (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f) -
        (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0f : 0.0f);
    const float vertical =
        (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS ? 1.0f : 0.0f) -
        (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS ? 1.0f : 0.0f);
    return {horizontal, vertical, forward};
}

void update_title(GLFWwindow* window, const Application& app) {
    std::ostringstream title;
    title << "Physics Sandbox | " << (app.paused ? "Paused" : "Running")
          << " | Bodies: " << app.world.bodies().size()
          << " | Gravity: " << app.gravity.y
          << " | Space pause, R reset, 1 sphere, 2 cube";
    glfwSetWindowTitle(window, title.str().c_str());
}

} // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW. Check that a display server is available.\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Physics Sandbox", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create an OpenGL 3.3 window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    Application app;
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (!app.renderer.initialize(framebufferWidth, framebufferHeight)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    double previousTime = glfwGetTime();
    double lastTitleUpdate = previousTime;
    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        const double currentTime = glfwGetTime();
        const float frameTime =
            static_cast<float>(std::clamp(currentTime - previousTime, 0.0, 0.1));
        previousTime = currentTime;

        app.camera.move(read_camera_input(window), frameTime);
        if (!app.paused) {
            app.world.step(frameTime);
        }

        const HudState hud{app.paused,
                           static_cast<int>(app.world.bodies().size()),
                           app.gravity,
                           app.spawnShape,
                           app.spawnMass,
                           app.spawnRestitution,
                           app.spawnFriction};
        app.renderer.draw(app.world, app.camera, hud);
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (currentTime - lastTitleUpdate >= 0.25) {
            update_title(window, app);
            lastTitleUpdate = currentTime;
        }
    }

    app.renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
