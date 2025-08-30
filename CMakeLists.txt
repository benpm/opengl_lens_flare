cmake_minimum_required(VERSION 3.16)

project(opengl_lens_flare 
    VERSION 1.0.0
    DESCRIPTION "Physically-based real-time lens flare rendering"
    LANGUAGES CXX
)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(ENABLE_TESTING "Build unit tests" OFF)
option(ENABLE_DOCS "Generate documentation" OFF)
option(ENABLE_PROFILING "Enable profiling support" OFF)

# Find required dependencies
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)

# Add GLFW as subdirectory
add_subdirectory(external/glfw)

# Dependency: glad
add_library(glad external/glad/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/glad/include)

# Create main executable
add_executable(${PROJECT_NAME}
    opengl_lens_flare.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    glfw
    glad
    glm::glm-header-only
)

# Compiler-specific options
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        GLEW_STATIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Testing support
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()