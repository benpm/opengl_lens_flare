# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a physically-based real-time lens flare rendering system implemented in OpenGL/GLSL, based on the research paper "Physically-Based Real-Time Lens Flare Rendering" by Hullin et al. The system simulates realistic lens flares by tracing light rays through a complete optical system model of camera lenses.

## Build Commands

### Basic Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows with vcpkg
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build with All Features
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_TESTING=ON \
         -DENABLE_DOCS=ON \
         -DENABLE_PROFILING=ON \
         -DENABLE_SANITIZERS=ON
```

### Testing
```bash
# From build directory
ctest --output-on-failure -C Release
```

## Dependencies

Required libraries:
- **OpenGL 4.3+**: Core graphics API
- **GLFW 3.0+**: Window management and input
- **GLEW 2.0+**: OpenGL extension loading
- **GLM 0.9.9+**: Mathematics library for graphics
- **Catch2 3.4.0**: Testing framework (optional, auto-downloaded)

Supported package managers: vcpkg, Conan, system packages

## Architecture Overview

### Core Rendering Pipeline
1. **Aperture Generation** (`renderAperture()`): Creates procedural aperture masks with configurable blade count
2. **Starburst Generation** (`generateStarburst()`): Produces diffraction patterns via FFT (simplified in this implementation)
3. **Lens Flare Rendering** (`renderLensFlare()`): Ray traces through optical system using compute shaders
4. **Tone Mapping** (`tonemap()`): Applies ACES tone mapping for final display

### Key Classes

**`LensFlareRenderer`**: Main rendering class containing:
- OpenGL resources (shaders, textures, buffers)
- Lens system data and ghost enumeration
- Multi-pass rendering pipeline
- Complete resource management

**`LensFlareDemo`**: Application wrapper providing:
- GLFW window management
- Mouse input for light direction control
- Main render loop

**`ShaderLibrary`**: Static shader source container

### Optical System Model

The system models a **Nikon 28-75mm lens** with:
- 29 optical surfaces (spherical and flat)
- Anti-reflective coating simulation
- Refractive index variations
- Aperture and diaphragm mechanics

**Ghost Generation**: Enumerates all 2-reflection sequences through the lens system, creating individual "ghosts" (lens flare elements) for each possible light path.

### GPU Implementation

**Compute Shaders**: Ray tracing performed on GPU with:
- Sparse ray bundle tracing (not full ray tracing)
- Structured buffer objects for lens interface data
- Parallel processing of multiple ghosts
- Interpolation between ray samples for beam-like effects

**Data Flow**:
```
Light Direction → Ray Generation → Lens System Traversal → 
Ghost Computation → Rasterization → HDR Accumulation → Tone Mapping
```

### Key Technical Details

- **Ray Bundle Approach**: Uses sparse uniform ray grids rather than dense ray tracing
- **Ghost Enumeration**: Pre-computes all possible 2-reflection sequences
- **Fresnel Equations**: Accurate reflection/transmission calculations
- **Chromatic Aberration**: Wavelength-dependent refractive indices
- **Diffraction Approximation**: Simplified FFT-based starburst generation

### Physical Accuracy vs Performance

The implementation balances physical accuracy with real-time performance:
- **Accurate**: Fresnel equations, lens geometry, coating effects
- **Approximated**: Full FFT diffraction, adaptive tessellation
- **Simplified**: Some wave-optical effects, polarization

### Coordinate Systems

- **World Space**: Standard 3D coordinates
- **Optical Axis**: Z-axis along lens system
- **Screen Space**: Final rendered coordinates
- **Aperture Coordinates**: Normalized aperture plane coordinates

## Important Implementation Notes

- The main executable combines all rendering components in a single file (`opengl_lens_flare.cpp`)
- CMakeLists.txt appears to contain multiple concatenated configuration files
- Shaders are embedded as string literals in the C++ code
- The system requires OpenGL 4.3+ for compute shader support
- Mouse interaction controls light source direction for interactive visualization