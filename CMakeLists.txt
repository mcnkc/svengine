cmake_minimum_required(VERSION 3.20)
project(physics_sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

enable_testing()

add_library(physics src/physics/physics_world.cpp)
target_include_directories(physics PUBLIC include)
target_link_libraries(physics PUBLIC glm::glm)

add_executable(physics_tests tests/physics_tests.cpp)
target_link_libraries(physics_tests PRIVATE physics)
add_test(NAME physics_tests COMMAND physics_tests)

add_executable(camera_tests tests/camera_tests.cpp)
add_library(camera src/app/camera.cpp)
target_include_directories(camera PUBLIC include)
target_link_libraries(camera PUBLIC glm::glm)
target_link_libraries(camera_tests PRIVATE camera)
add_test(NAME camera_tests COMMAND camera_tests)

add_library(renderer src/render/renderer.cpp)
target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC physics camera glm::glm OpenGL::GL)

add_executable(physics_sandbox src/main.cpp)
target_link_libraries(physics_sandbox PRIVATE renderer glfw OpenGL::GL)
