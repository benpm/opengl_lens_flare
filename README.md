# Physically-Based Real-Time Lens Flare Rendering
OpenGL/GLSL implementation of the physically-based lens flare rendering system. Following are the key components and how they correspond to the original paper. For the full paper, see the file "Physically-Based Real-Time Lens Flare Rendering.xml"

## Core Implementation Features:

**1. Physical Ray Tracing (Section 4.1)**
- Compute shader implements the sparse ray bundle tracing through the lens system
- Each ghost corresponds to specific reflection sequences as described in the paper
- Fresnel equations for reflection/transmission at each interface

**2. Lens System Modeling (Section 3)**
- Complete Nikon 28-75mm lens specification with real optical parameters
- Support for both spherical and flat surfaces
- Anti-reflective coating simulation using the FresnelAR function
- Proper handling of refractive index variations

**3. Aperture and Diffraction (Section 3.3)**
- Procedural aperture generation with configurable blade count
- Starburst pattern generation (simplified - full FFT implementation would require additional compute shaders)
- Support for aperture imperfections and dust effects

**4. GPU Acceleration (Section 4.3)**
- OpenGL compute shaders for parallel ray tracing
- Shader Storage Buffer Objects (SSBOs) for lens interface and ghost data
- Efficient memory management with proper buffer binding

**5. Real-time Rendering Pipeline**
- Multi-pass rendering: aperture → starburst → lens flare → tonemap
- HDR rendering with ACES tone mapping
- Additive blending for realistic light accumulation

## Key Differences from DirectX Version:

**OpenGL-Specific Features:**
- Uses GLSL compute shaders instead of HLSL
- Shader Storage Buffer Objects (SSBOs) instead of structured buffers
- OpenGL framebuffer objects for render targets
- GLFW for window management and input handling

**Simplified Elements:**
- FFT implementation simplified for clarity (full butterfly FFT would require additional compute passes)
- Some optimization features like adaptive tessellation are framework-ready but not fully implemented
- Focus on core physically-based rendering rather than all performance optimizations

## Usage Example:
The implementation includes a complete demo class that:
- Sets up OpenGL context with GLFW
- Handles mouse input to control light direction
- Renders the lens flare effects in real-time
- Provides proper cleanup and resource management

This OpenGL port maintains the paper's physically-based approach while being more accessible and portable across different platforms than the original DirectX implementation.

# Build System

## Main Features:

**1. Cross-Platform Support**
- Windows (Visual Studio, MinGW)
- Linux (GCC, Clang)
- macOS (Xcode, Unix Makefiles)

**2. Dependency Management**
- Automatic detection of OpenGL, GLFW, GLEW, GLM
- Support for both system packages and vcpkg/Conan
- Fallback mechanisms for different package managers

**3. Build Configurations**
- Debug and Release builds with appropriate compiler flags
- Optional features (testing, documentation, profiling)
- Sanitizer support for development builds

**4. Testing Framework**
- Integration with Catch2 testing framework
- Automatic test discovery with CTest
- CI/CD ready configuration

**5. Installation and Packaging**
- Proper installation targets for system-wide deployment
- Package generation (DEB, RPM, NSIS, DMG)
- Resource copying for shaders and assets

## Directory Structure:
```
lens-flare-renderer/
├── CMakeLists.txt                 # Main build file
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── lens_flare_renderer.cpp
│   └── ...
├── include/
│   ├── lens_flare_renderer.h
│   ├── version.h.in
│   └── ...
├── shaders/
│   ├── lens.comp
│   ├── aperture.frag
│   └── ...
├── assets/
│   ├── dust.png
│   └── ...
├── tests/
│   ├── CMakeLists.txt
│   └── test_*.cpp
├── cmake/
│   ├── modules/FindGLEW.cmake
│   └── LensFlareRendererConfig.cmake.in
├── docs/
├── vcpkg.json                     # vcpkg dependencies
├── conanfile.txt                  # Conan dependencies
└── .github/workflows/ci.yml       # CI configuration
```

## Usage Examples:

**Basic Build:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

**With All Features:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_TESTING=ON \
         -DENABLE_DOCS=ON \
         -DENABLE_PROFILING=ON \
         -DENABLE_SANITIZERS=ON
```

**Windows with vcpkg:**
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Key Benefits:

1. **Modern CMake**: Uses target-based approach with proper visibility rules
2. **Flexible Dependencies**: Works with system packages, vcpkg, Conan
3. **Development Tools**: Testing, profiling, sanitizers, documentation
4. **CI Ready**: Includes GitHub Actions workflow
5. **Professional Packaging**: Creates installers and packages for distribution

The build system is production-ready and follows CMake best practices for a graphics application with OpenGL dependencies.