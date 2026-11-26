#define GL_GLEXT_PROTOTYPES
#include "render/renderer.hpp"

#include <GL/glcorearb.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int shadowMapSize = 1024;
constexpr float sceneExtent = 50.0f;
constexpr float gridExtent = 25.0f;
constexpr float gridSpacing = 1.0f;

const char* sceneVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpace;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec4 vLightPosition;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vLightPosition = uLightSpace * worldPosition;
    gl_Position = uProjection * uView * worldPosition;
}
)glsl";

const char* sceneFragmentShader = R"glsl(
#version 330 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec4 vLightPosition;

uniform vec3 uBaseColor;
uniform vec3 uLightDirection;
uniform sampler2D uShadowMap;
uniform int uEmissive;

out vec4 fragmentColor;

float shadowAmount(vec4 lightPosition, vec3 normal) {
    vec3 projected = lightPosition.xyz / lightPosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.z > 1.0 || projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0) {
        return 0.0;
    }

    float bias = max(0.0015 * (1.0 - dot(normal, uLightDirection)), 0.0004);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(uShadowMap, projected.xy + vec2(x, y) * texel).r;
            shadow += projected.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    if (uEmissive == 1) {
        fragmentColor = vec4(uBaseColor, 1.0);
        return;
    }

    vec3 normal = normalize(vWorldNormal);
    float diffuse = max(dot(normal, uLightDirection), 0.0);
    float shadow = shadowAmount(vLightPosition, normal);
    vec3 lighting = vec3(0.28) + (1.0 - shadow) * diffuse * vec3(0.72);
    fragmentColor = vec4(uBaseColor * lighting, 1.0);
}
)glsl";

const char* depthVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uLightSpace;
void main() {
    gl_Position = uLightSpace * uModel * vec4(aPosition, 1.0);
}
)glsl";

const char* depthFragmentShader = R"glsl(
#version 330 core
void main() {}
)glsl";

const char* uiVertexShader = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)glsl";

const char* uiFragmentShader = R"glsl(
#version 330 core
in vec4 vColor;
out vec4 fragmentColor;
void main() {
    fragmentColor = vColor;
}
)glsl";

struct Mesh {
    GLuint vao{0};
    GLuint vbo{0};
    GLsizei count{0};
    GLenum primitive{GL_TRIANGLES};
};

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "OpenGL shader compilation failed: " << log.data() << '\n';
    glDeleteShader(shader);
    return 0;
}

GLuint create_program(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return 0;
    }
    const GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_TRUE) {
        return program;
    }

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    std::cerr << "OpenGL shader link failed: " << log.data() << '\n';
    glDeleteProgram(program);
    return 0;
}

void append_vertex(std::vector<float>& vertices, const glm::vec3& position,
                   const glm::vec3& normal) {
    vertices.insert(vertices.end(), {position.x, position.y, position.z,
                                     normal.x, normal.y, normal.z});
}

void append_triangle(std::vector<float>& vertices, const glm::vec3& a,
                     const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& normal) {
    append_vertex(vertices, a, normal);
    append_vertex(vertices, b, normal);
    append_vertex(vertices, c, normal);
}

void append_quad(std::vector<float>& vertices, const glm::vec3& a,
                 const glm::vec3& b, const glm::vec3& c,
                 const glm::vec3& d, const glm::vec3& normal) {
    append_triangle(vertices, a, b, c, normal);
    append_triangle(vertices, a, c, d, normal);
}

Mesh create_mesh(const std::vector<float>& vertices, GLenum primitive) {
    Mesh mesh;
    mesh.count = static_cast<GLsizei>(vertices.size() / 6);
    mesh.primitive = primitive;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return mesh;
}

std::vector<float> make_cube_vertices() {
    std::vector<float> vertices;
    const float n = -0.5f;
    const float p = 0.5f;
    append_quad(vertices, {n, n, p}, {p, n, p}, {p, p, p}, {n, p, p}, {0, 0, 1});
    append_quad(vertices, {p, n, n}, {n, n, n}, {n, p, n}, {p, p, n}, {0, 0, -1});
    append_quad(vertices, {p, n, p}, {p, n, n}, {p, p, n}, {p, p, p}, {1, 0, 0});
    append_quad(vertices, {n, n, n}, {n, n, p}, {n, p, p}, {n, p, n}, {-1, 0, 0});
    append_quad(vertices, {n, p, p}, {p, p, p}, {p, p, n}, {n, p, n}, {0, 1, 0});
    append_quad(vertices, {n, n, n}, {p, n, n}, {p, n, p}, {n, n, p}, {0, -1, 0});
    return vertices;
}

std::vector<float> make_sphere_vertices() {
    std::vector<float> vertices;
    constexpr int stacks = 16;
    constexpr int slices = 24;
    constexpr float pi = 3.14159265358979323846f;

    auto point = [](float phi, float theta) {
        return glm::vec3(std::sin(phi) * std::cos(theta),
                         std::cos(phi),
                         std::sin(phi) * std::sin(theta));
    };

    for (int stack = 0; stack < stacks; ++stack) {
        const float phi0 = pi * static_cast<float>(stack) / stacks;
        const float phi1 = pi * static_cast<float>(stack + 1) / stacks;
        for (int slice = 0; slice < slices; ++slice) {
            const float theta0 = 2.0f * pi * static_cast<float>(slice) / slices;
            const float theta1 = 2.0f * pi * static_cast<float>(slice + 1) / slices;
            const glm::vec3 a = point(phi0, theta0);
            const glm::vec3 b = point(phi1, theta0);
            const glm::vec3 c = point(phi1, theta1);
            const glm::vec3 d = point(phi0, theta1);
            append_triangle(vertices, a, b, c, glm::normalize(a + b + c));
            append_triangle(vertices, a, c, d, glm::normalize(a + c + d));
        }
    }
    return vertices;
}

std::vector<float> make_ground_vertices() {
    std::vector<float> vertices;
    const float e = sceneExtent;
    append_quad(vertices, {-e, 0.0f, -e}, {e, 0.0f, -e},
                {e, 0.0f, e}, {-e, 0.0f, e}, {0.0f, 1.0f, 0.0f});
    return vertices;
}

std::vector<float> make_grid_vertices() {
    std::vector<float> vertices;
    for (float position = -gridExtent; position <= gridExtent; position += gridSpacing) {
        append_vertex(vertices, {position, 0.008f, -gridExtent}, {0.0f, 1.0f, 0.0f});
        append_vertex(vertices, {position, 0.008f, gridExtent}, {0.0f, 1.0f, 0.0f});
        append_vertex(vertices, {-gridExtent, 0.008f, position}, {0.0f, 1.0f, 0.0f});
        append_vertex(vertices, {gridExtent, 0.008f, position}, {0.0f, 1.0f, 0.0f});
    }
    return vertices;
}

struct UiVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

using Glyph = std::array<unsigned char, 7>;

Glyph glyph_for(char character) {
    switch (std::toupper(static_cast<unsigned char>(character))) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '+': return {0, 4, 4, 31, 4, 4, 0};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case '|': return {4, 4, 4, 4, 4, 4, 4};
    default: return {};
    }
}

void append_ui_rect(std::vector<UiVertex>& vertices, float x, float y,
                    float width, float height, const std::array<float, 4>& color,
                    int viewportWidth, int viewportHeight) {
    const float left = 2.0f * x / viewportWidth - 1.0f;
    const float right = 2.0f * (x + width) / viewportWidth - 1.0f;
    const float top = 1.0f - 2.0f * y / viewportHeight;
    const float bottom = 1.0f - 2.0f * (y + height) / viewportHeight;
    const UiVertex a{left, top, color[0], color[1], color[2], color[3]};
    const UiVertex b{right, top, color[0], color[1], color[2], color[3]};
    const UiVertex c{right, bottom, color[0], color[1], color[2], color[3]};
    const UiVertex d{left, bottom, color[0], color[1], color[2], color[3]};
    vertices.insert(vertices.end(), {a, b, c, a, c, d});
}

void append_ui_text(std::vector<UiVertex>& vertices, const std::string& text,
                    float x, float y, float scale, const std::array<float, 4>& color,
                    int viewportWidth, int viewportHeight) {
    float cursorX = x;
    for (char character : text) {
        const Glyph glyph = glyph_for(character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[static_cast<std::size_t>(row)] & (1U << (4 - column))) != 0) {
                    append_ui_rect(vertices, cursorX + column * scale, y + row * scale,
                                   scale, scale, color, viewportWidth, viewportHeight);
                }
            }
        }
        cursorX += 6.0f * scale;
    }
}

std::string format_value(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

void delete_mesh(Mesh& mesh) {
    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
    }
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
    }
    mesh = {};
}

} // namespace

struct Renderer::State {
    GLuint sceneProgram{0};
    GLuint depthProgram{0};
    GLuint uiProgram{0};
    GLuint shadowFramebuffer{0};
    GLuint shadowTexture{0};
    GLuint uiVao{0};
    GLuint uiVbo{0};
    Mesh cube;
    Mesh sphere;
    Mesh ground;
    Mesh grid;
    int width{1};
    int height{1};
    bool initialized{false};
};

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int width, int height) {
    shutdown();
    state_ = std::make_unique<State>();
    resize(width, height);

    state_->sceneProgram = create_program(sceneVertexShader, sceneFragmentShader);
    state_->depthProgram = create_program(depthVertexShader, depthFragmentShader);
    state_->uiProgram = create_program(uiVertexShader, uiFragmentShader);
    if (state_->sceneProgram == 0 || state_->depthProgram == 0 || state_->uiProgram == 0) {
        std::cerr << "Renderer initialization failed while compiling shaders\n";
        shutdown();
        return false;
    }

    state_->cube = create_mesh(make_cube_vertices(), GL_TRIANGLES);
    state_->sphere = create_mesh(make_sphere_vertices(), GL_TRIANGLES);
    state_->ground = create_mesh(make_ground_vertices(), GL_TRIANGLES);
    state_->grid = create_mesh(make_grid_vertices(), GL_LINES);

    glGenFramebuffers(1, &state_->shadowFramebuffer);
    glGenTextures(1, &state_->shadowTexture);
    glBindTexture(GL_TEXTURE_2D, state_->shadowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, state_->shadowFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           state_->shadowTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Renderer initialization failed: shadow framebuffer status 0x"
                  << std::hex << framebufferStatus << std::dec << '\n';
        shutdown();
        return false;
    }

    glGenVertexArrays(1, &state_->uiVao);
    glGenBuffers(1, &state_->uiVbo);
    glBindVertexArray(state_->uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, state_->uiVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    state_->initialized = true;
    return true;
}

void Renderer::resize(int width, int height) {
    if (!state_) {
        return;
    }
    state_->width = std::max(width, 1);
    state_->height = std::max(height, 1);
    glViewport(0, 0, state_->width, state_->height);
}

void Renderer::draw(const PhysicsWorld& world, const Camera& camera, const HudState& hud) {
    if (!state_ || !state_->initialized) {
        return;
    }

    const glm::vec3 lightDirection = glm::normalize(glm::vec3(0.55f, -1.0f, 0.4f));
    const glm::mat4 lightView = glm::lookAt(-lightDirection * 35.0f,
                                            glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProjection = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f,
                                                 1.0f, 80.0f);
    const glm::mat4 lightSpace = lightProjection * lightView;
    const glm::mat4 view = camera.viewMatrix();
    const float aspect = static_cast<float>(state_->width) / state_->height;
    const glm::mat4 projection =
        glm::perspective(glm::radians(camera.fieldOfView()), aspect, 0.1f, 120.0f);

    glViewport(0, 0, shadowMapSize, shadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, state_->shadowFramebuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(state_->depthProgram);
    glUniformMatrix4fv(glGetUniformLocation(state_->depthProgram, "uLightSpace"),
                       1, GL_FALSE, glm::value_ptr(lightSpace));
    const GLint depthModelLocation = glGetUniformLocation(state_->depthProgram, "uModel");
    auto drawDepth = [&](const Mesh& mesh, const glm::mat4& model) {
        glUniformMatrix4fv(depthModelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(mesh.vao);
        glDrawArrays(mesh.primitive, 0, mesh.count);
    };
    drawDepth(state_->ground, glm::mat4(1.0f));
    for (const RigidBody& body : world.bodies()) {
        const glm::vec3 scale = body.shape == ShapeType::Sphere
                                    ? glm::vec3(body.radius * 2.0f)
                                    : body.halfExtents * 2.0f;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), body.position) *
                                glm::scale(glm::mat4(1.0f), scale);
        drawDepth(body.shape == ShapeType::Sphere ? state_->sphere : state_->cube, model);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, state_->width, state_->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(state_->sceneProgram);
    glUniformMatrix4fv(glGetUniformLocation(state_->sceneProgram, "uView"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(state_->sceneProgram, "uProjection"),
                       1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(state_->sceneProgram, "uLightSpace"),
                       1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform3fv(glGetUniformLocation(state_->sceneProgram, "uLightDirection"),
                 1, glm::value_ptr(-lightDirection));
    glUniform1i(glGetUniformLocation(state_->sceneProgram, "uShadowMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state_->shadowTexture);

    const GLint modelLocation = glGetUniformLocation(state_->sceneProgram, "uModel");
    const GLint colorLocation = glGetUniformLocation(state_->sceneProgram, "uBaseColor");
    const GLint emissiveLocation = glGetUniformLocation(state_->sceneProgram, "uEmissive");
    auto drawLit = [&](const Mesh& mesh, const glm::mat4& model,
                       const glm::vec3& color, bool emissive = false) {
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(colorLocation, 1, glm::value_ptr(color));
        glUniform1i(emissiveLocation, emissive ? 1 : 0);
        glBindVertexArray(mesh.vao);
        glDrawArrays(mesh.primitive, 0, mesh.count);
    };

    drawLit(state_->ground, glm::mat4(1.0f), {0.24f, 0.27f, 0.30f});
    drawLit(state_->grid, glm::mat4(1.0f), {0.10f, 0.12f, 0.14f});
    for (const RigidBody& body : world.bodies()) {
        const glm::vec3 scale = body.shape == ShapeType::Sphere
                                    ? glm::vec3(body.radius * 2.0f)
                                    : body.halfExtents * 2.0f;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), body.position) *
                                glm::scale(glm::mat4(1.0f), scale);
        const glm::vec3 color = body.shape == ShapeType::Sphere
                                    ? glm::vec3(0.12f, 0.52f, 0.92f)
                                    : glm::vec3(0.95f, 0.42f, 0.16f);
        drawLit(body.shape == ShapeType::Sphere ? state_->sphere : state_->cube,
                model, color);
    }

    const glm::vec3 sunPosition = -lightDirection * 27.0f;
    const glm::mat4 sunModel = glm::translate(glm::mat4(1.0f), sunPosition) *
                               glm::scale(glm::mat4(1.0f), glm::vec3(1.1f));
    drawLit(state_->sphere, sunModel, {1.0f, 0.82f, 0.40f}, true);

    std::vector<UiVertex> uiVertices;
    const std::array<float, 4> panelColor{0.025f, 0.035f, 0.055f, 0.82f};
    const std::array<float, 4> textColor{0.86f, 0.91f, 0.98f, 1.0f};
    append_ui_rect(uiVertices, 10.0f, 10.0f, 1080.0f, 124.0f,
                   panelColor, state_->width, state_->height);

    const std::string shape = hud.spawnShape == ShapeType::Sphere ? "SPHERE" : "CUBE";
    const std::array<std::string, 5> lines{
        std::string("PHYSICS SANDBOX | ") + (hud.paused ? "PAUSED" : "RUNNING") +
            " | BODIES " + std::to_string(hud.bodyCount),
        "GRAV Y " + format_value(hud.gravity.y) + " | SPAWN " + shape +
            " | MASS " + format_value(hud.mass) + " | REST " +
            format_value(hud.restitution) + " | FRIC " + format_value(hud.friction),
        "1 SPHERE 2 CUBE | SPACE PAUSE | R RESET | [ ] GRAVITY",
        "COMMA/PERIOD MASS | SEMI/APOS REST | N/M FRICTION",
        "WASD MOVE E/Q UP/DOWN | RMB LOOK | WHEEL ZOOM"
    };
    for (std::size_t i = 0; i < lines.size(); ++i) {
        append_ui_text(uiVertices, lines[i], 20.0f, 18.0f + i * 22.0f,
                       2.0f, textColor, state_->width, state_->height);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(state_->uiProgram);
    glBindVertexArray(state_->uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, state_->uiVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(uiVertices.size() * sizeof(UiVertex)),
                 uiVertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(uiVertices.size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::shutdown() {
    if (!state_) {
        return;
    }
    delete_mesh(state_->cube);
    delete_mesh(state_->sphere);
    delete_mesh(state_->ground);
    delete_mesh(state_->grid);
    if (state_->sceneProgram != 0) glDeleteProgram(state_->sceneProgram);
    if (state_->depthProgram != 0) glDeleteProgram(state_->depthProgram);
    if (state_->uiProgram != 0) glDeleteProgram(state_->uiProgram);
    if (state_->shadowTexture != 0) glDeleteTextures(1, &state_->shadowTexture);
    if (state_->shadowFramebuffer != 0) glDeleteFramebuffers(1, &state_->shadowFramebuffer);
    if (state_->uiVbo != 0) glDeleteBuffers(1, &state_->uiVbo);
    if (state_->uiVao != 0) glDeleteVertexArrays(1, &state_->uiVao);
    state_.reset();
}
