#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <cmath>
#include <cassert>

// OpenGL error checking macro
#define CHECK_GL_ERROR() \
    do { \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) { \
            std::cerr << "OpenGL error " << err << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            assert(false); \
        } \
    } while (0)

// Constants
#define PI 3.14159265359f
#define TWOPI 6.28318530718f
#define NANO_METER 0.0000001f
#define INCOMING_LIGHT_TEMP 6000.0f

// Structures matching the original implementation
struct PatentFormat {
    float r;    // radius
    float d;    // distance
    float n;    // refractive index
    bool f;     // flat surface flag
    float w;    // width
    float h;    // height
    float c;    // coating parameter
};

struct LensInterface {
    glm::vec3 center;
    float radius;
    glm::vec3 n;        // n.x = left IOR, n.y = coating IOR, n.z = right IOR
    float sa;           // surface aperture
    float d1;           // coating thickness
    float flat;         // flat surface flag
    float pos;          // position along optical axis
    float w;            // width factor
};

struct GhostData {
    float bounce1;
    float bounce2;
    float padding1;
    float padding2;
};

struct GlobalUniforms {
    float time;
    float spread;
    float plate_size;
    float aperture_id;
    
    float num_interfaces;
    float coating_quality;
    glm::vec2 backbuffer_size;
    
    glm::vec3 light_dir;
    float aperture_resolution;
    
    float aperture_opening;
    float number_of_blades;
    float starburst_resolution;
    float padding;
};

class LensFlareRenderer {
private:
    // OpenGL resources
    GLuint program_lens_flare;
    GLuint program_aperture;
    GLuint program_starburst;
    GLuint program_tonemap;
    GLuint program_fft_row, program_fft_col;
    
    // Textures
    GLuint texture_hdr;
    GLuint texture_aperture;
    GLuint texture_starburst;
    GLuint texture_dust;
    GLuint texture_fft_real[2];
    GLuint texture_fft_imag[2];
    
    // Framebuffers
    GLuint fbo_hdr;
    GLuint fbo_aperture;
    GLuint fbo_starburst;
    
    // Buffers
    GLuint ssbo_lens_interfaces;
    GLuint ssbo_ghost_data;
    GLuint ssbo_vertex_data;
    GLuint ubo_globals;
    
    // Vertex data
    GLuint vao_quad;
    GLuint vbo_quad;
    GLuint ebo_quad;
    
    // Lens system data
    std::vector<LensInterface> lens_interfaces;
    std::vector<GhostData> ghost_data;
    GlobalUniforms globals;
    
    // Configuration
    int aperture_resolution = 512;
    int starburst_resolution = 2048;
    int patch_tessellation = 32;
    int num_ghosts = 0;
    
public:
    LensFlareRenderer() {
        std::cout << "LensFlareRenderer: Starting initialization..." << std::endl;
        
        std::cout << "LensFlareRenderer: Initializing lens system..." << std::endl;
        initializeLensSystem();
        
        std::cout << "LensFlareRenderer: Setting up OpenGL..." << std::endl;
        setupOpenGL();
        
        std::cout << "LensFlareRenderer: Creating shaders..." << std::endl;
        createShaders();
        
        std::cout << "LensFlareRenderer: Setting up textures..." << std::endl;
        setupTextures();
        
        std::cout << "LensFlareRenderer: Setting up buffers..." << std::endl;
        setupBuffers();
        
        std::cout << "LensFlareRenderer: Initialization complete!" << std::endl;
    }
    
    ~LensFlareRenderer() {
        cleanup();
    }
    
    void render(float time, const glm::vec3& light_direction) {
        updateUniforms(time, light_direction);
        
        // 1. Generate aperture mask
        renderAperture();
        
        // 2. Generate starburst via FFT
        generateStarburst();
        
        // 3. Render lens flare ghosts
        renderLensFlare();
        
        // 4. Tonemap final result
        tonemap();
    }
    
private:
    void initializeLensSystem() {
        // Nikon 28-75mm lens data (from original implementation)
        std::vector<PatentFormat> nikon_lens = {
            {72.747f, 2.300f, 1.60300f, false, 0.2f, 29.0f, 530},
            {37.000f, 13.000f, 1.00000f, false, 0.2f, 29.0f, 600},
            {-172.809f, 2.100f, 1.58913f, false, 2.7f, 26.2f, 570},
            {39.894f, 1.000f, 1.00000f, false, 2.7f, 26.2f, 660},
            {49.820f, 4.400f, 1.86074f, false, 0.5f, 20.0f, 330},
            {74.750f, 53.142f, 1.00000f, false, 0.5f, 20.0f, 544},
            {63.402f, 1.600f, 1.86074f, false, 0.5f, 16.1f, 740},
            {37.530f, 8.600f, 1.51680f, false, 0.5f, 16.1f, 411},
            {-75.887f, 1.600f, 1.80458f, false, 0.5f, 16.0f, 580},
            {-97.792f, 7.063f, 1.00000f, false, 0.5f, 16.5f, 730},
            {96.034f, 3.600f, 1.62041f, false, 0.5f, 18.0f, 700},
            {261.743f, 0.100f, 1.00000f, false, 0.5f, 18.0f, 440},
            {54.262f, 6.000f, 1.69680f, false, 0.5f, 18.0f, 800},
            {-5995.277f, 1.532f, 1.00000f, false, 0.5f, 18.0f, 300},
            {0.0f, 2.800f, 1.00000f, true, 18.0f, 7.0f, 440}, // Aperture
            {-74.414f, 2.200f, 1.90265f, false, 0.5f, 13.0f, 500},
            {-62.929f, 1.450f, 1.51680f, false, 0.1f, 13.0f, 770},
            {121.380f, 2.500f, 1.00000f, false, 4.0f, 13.1f, 820},
            {-85.723f, 1.400f, 1.49782f, false, 4.0f, 13.0f, 200},
            {31.093f, 2.600f, 1.80458f, false, 4.0f, 13.1f, 540},
            {84.758f, 16.889f, 1.00000f, false, 0.5f, 13.0f, 580},
            {459.690f, 1.400f, 1.86074f, false, 1.0f, 15.0f, 533},
            {40.240f, 7.300f, 1.49782f, false, 1.0f, 15.0f, 666},
            {-49.771f, 0.100f, 1.00000f, false, 1.0f, 15.2f, 500},
            {62.369f, 7.000f, 1.67025f, false, 1.0f, 16.0f, 487},
            {-76.454f, 5.200f, 1.00000f, false, 1.0f, 16.0f, 671},
            {-32.524f, 2.000f, 1.80454f, false, 0.5f, 17.0f, 487},
            {-50.194f, 39.683f, 1.00000f, false, 0.5f, 17.0f, 732},
            {0.0f, 5.0f, 1.00000f, true, 10.0f, 10.0f, 500}
        };
        
        // Convert patent format to lens interfaces
        float total_distance = 0.0f;
        lens_interfaces.clear();
        
        for (int i = nikon_lens.size() - 1; i >= 0; --i) {
            const auto& entry = nikon_lens[i];
            total_distance += entry.d;
            
            LensInterface interface;
            interface.center = glm::vec3(0.0f, 0.0f, total_distance - entry.r);
            interface.radius = entry.r;
            
            float left_ior = (i == 0) ? 1.0f : nikon_lens[i-1].n;
            interface.n = glm::vec3(left_ior, 1.0f, entry.n);
            
            interface.sa = entry.h;
            interface.d1 = entry.c;
            interface.flat = entry.f ? 1.0f : 0.0f;
            interface.pos = total_distance;
            interface.w = entry.w;
            
            lens_interfaces.push_back(interface);
        }
        
        // Generate ghost data (all possible 2-reflection sequences)
        ghost_data.clear();
        for (int bounce2 = 1; bounce2 < lens_interfaces.size() - 1; ++bounce2) {
            for (int bounce1 = bounce2 + 2; bounce1 < lens_interfaces.size() - 1; ++bounce1) {
                GhostData ghost;
                ghost.bounce1 = static_cast<float>(bounce1);
                ghost.bounce2 = static_cast<float>(bounce2);
                ghost.padding1 = 0.0f;
                ghost.padding2 = 0.0f;
                ghost_data.push_back(ghost);
            }
        }
        
        num_ghosts = ghost_data.size();
    }
    
    void setupOpenGL() {
        std::cout << "  Initializing GLAD..." << std::endl;
        // Initialize GLAD
        if (!gladLoadGL(glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }
        
        std::cout << "  OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "  OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "  OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        CHECK_GL_ERROR();
        
        // Enable required OpenGL features
        std::cout << "  Enabling OpenGL features..." << std::endl;
        glEnable(GL_BLEND);
        CHECK_GL_ERROR();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for lens flare
        CHECK_GL_ERROR();
        glEnable(GL_DEPTH_TEST);
        CHECK_GL_ERROR();
        
        // Create fullscreen quad
        std::cout << "  Creating fullscreen quad..." << std::endl;
        float quad_vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 1.0f
        };
        
        GLuint quad_indices[] = {
            0, 1, 2,
            2, 3, 0
        };
        
        std::cout << "  Generating vertex arrays and buffers..." << std::endl;
        glGenVertexArrays(1, &vao_quad);
        CHECK_GL_ERROR();
        glGenBuffers(1, &vbo_quad);
        CHECK_GL_ERROR();
        glGenBuffers(1, &ebo_quad);
        CHECK_GL_ERROR();
        glGenBuffers(1, &ubo_globals);  // Generate UBO first!
        CHECK_GL_ERROR();
        
        std::cout << "  Setting up vertex array..." << std::endl;
        glBindVertexArray(vao_quad);
        CHECK_GL_ERROR();
        
        // Setup VBO
        glBindBuffer(GL_ARRAY_BUFFER, vbo_quad);
        CHECK_GL_ERROR();
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
        CHECK_GL_ERROR();
        
        // Setup EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_quad);
        CHECK_GL_ERROR();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);
        CHECK_GL_ERROR();
        
        // Setup vertex attributes
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        CHECK_GL_ERROR();
        glEnableVertexAttribArray(0);
        CHECK_GL_ERROR();
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        CHECK_GL_ERROR();
        glEnableVertexAttribArray(1);
        CHECK_GL_ERROR();
        
        std::cout << "  Setting up uniform buffer..." << std::endl;
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_globals);
        CHECK_GL_ERROR();
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalUniforms), nullptr, GL_DYNAMIC_DRAW);
        CHECK_GL_ERROR();
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_globals);
        CHECK_GL_ERROR();
        
        std::cout << "  Creating SSBO for lens interfaces (size: " << lens_interfaces.size() << ")..." << std::endl;
        glGenBuffers(1, &ssbo_lens_interfaces);
        CHECK_GL_ERROR();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_lens_interfaces);
        CHECK_GL_ERROR();
        if (!lens_interfaces.empty()) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, lens_interfaces.size() * sizeof(LensInterface), lens_interfaces.data(), GL_STATIC_DRAW);
            CHECK_GL_ERROR();
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lens_interfaces);
        CHECK_GL_ERROR();
        
        std::cout << "  Creating SSBO for ghost data (size: " << ghost_data.size() << ")..." << std::endl;
        glGenBuffers(1, &ssbo_ghost_data);
        CHECK_GL_ERROR();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_ghost_data);
        CHECK_GL_ERROR();
        if (!ghost_data.empty()) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, ghost_data.size() * sizeof(GhostData), ghost_data.data(), GL_STATIC_DRAW);
            CHECK_GL_ERROR();
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_ghost_data);
        CHECK_GL_ERROR();
        
        std::cout << "  Creating SSBO for vertex data (vertices: " << num_ghosts * patch_tessellation * patch_tessellation << ")..." << std::endl;
        int total_vertices = num_ghosts * patch_tessellation * patch_tessellation;
        glGenBuffers(1, &ssbo_vertex_data);
        CHECK_GL_ERROR();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_vertex_data);
        CHECK_GL_ERROR();
        if (total_vertices > 0) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, total_vertices * 4 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
            CHECK_GL_ERROR();
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_vertex_data);
        CHECK_GL_ERROR();
        
        std::cout << "  Unbinding buffers..." << std::endl;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        CHECK_GL_ERROR();
        glBindVertexArray(0);
        CHECK_GL_ERROR();
        
        std::cout << "  OpenGL setup complete!" << std::endl;
    }
    
    void renderAperture() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_aperture);
        glViewport(0, 0, aperture_resolution, aperture_resolution);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program_aperture);
        
        // Set aperture uniforms
        glUniform1f(glGetUniformLocation(program_aperture, "aperture_opening"), globals.aperture_opening);
        glUniform1f(glGetUniformLocation(program_aperture, "number_of_blades"), globals.number_of_blades);
        glUniform1f(glGetUniformLocation(program_aperture, "time"), globals.time);
        
        // Render fullscreen quad
        glBindVertexArray(vao_quad);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void generateStarburst() {
        // For this simplified version, we'll skip the FFT implementation
        // and use a procedural starburst instead
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_starburst);
        glViewport(0, 0, starburst_resolution, starburst_resolution);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Simple procedural starburst would go here
        // In a full implementation, this would involve FFT processing of the aperture
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void renderLensFlare() {
        // Update global uniforms
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_globals);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GlobalUniforms), &globals);
        
        // Run compute shader to trace rays
        glUseProgram(program_lens_flare);
        
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lens_interfaces);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_ghost_data);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_vertex_data);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_aperture);
        glUniform1i(glGetUniformLocation(program_lens_flare, "aperture_texture"), 0);
        
        // Dispatch compute shader
        int groups_x = (patch_tessellation + 15) / 16;
        int groups_y = (patch_tessellation + 15) / 16;
        glDispatchCompute(num_ghosts * groups_x, groups_y, 1);
        
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // Render to HDR buffer
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_hdr);
        glViewport(0, 0, 1920, 1080);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Here we would render the traced rays as triangulated patches
        // For simplicity, this is omitted but would involve creating geometry
        // from the compute shader results and rendering with additive blending
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void tonemap() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, 1920, 1080);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program_tonemap);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_hdr);
        glUniform1i(glGetUniformLocation(program_tonemap, "hdr_texture"), 0);
        
        glBindVertexArray(vao_quad);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    void updateUniforms(float time, const glm::vec3& light_direction) {
        globals.time = time;
        globals.spread = 0.75f;
        globals.plate_size = 10.0f;
        globals.aperture_id = 14.0f;
        globals.num_interfaces = static_cast<float>(lens_interfaces.size());
        globals.coating_quality = 1.25f;
        globals.backbuffer_size = glm::vec2(1920.0f, 1080.0f);
        globals.light_dir = light_direction;
        globals.aperture_resolution = static_cast<float>(aperture_resolution);
        globals.aperture_opening = 7.0f;
        globals.number_of_blades = 6.0f;
        globals.starburst_resolution = static_cast<float>(starburst_resolution);
    }
    
    std::string getVertexShaderSource() {
        return R"(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 uv;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aTexCoord;
}
)";
    }
    
    GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Program linking failed: " << infoLog << std::endl;
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        return program;
    }
    
    GLuint createComputeProgram(const std::string& computeSource) {
        GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);
        
        GLuint program = glCreateProgram();
        glAttachShader(program, computeShader);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Compute program linking failed: " << infoLog << std::endl;
        }
        
        glDeleteShader(computeShader);
        return program;
    }
    
    GLuint compileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        }
        
        return shader;
    }
    
    void cleanup() {
        // Clean up OpenGL resources
        glDeleteProgram(program_lens_flare);
        glDeleteProgram(program_aperture);
        glDeleteProgram(program_starburst);
        glDeleteProgram(program_tonemap);
        
        glDeleteTextures(1, &texture_hdr);
        glDeleteTextures(1, &texture_aperture);
        glDeleteTextures(1, &texture_starburst);
        glDeleteTextures(1, &texture_dust);
        
        glDeleteFramebuffers(1, &fbo_hdr);
        glDeleteFramebuffers(1, &fbo_aperture);
        glDeleteFramebuffers(1, &fbo_starburst);
        
        glDeleteBuffers(1, &ssbo_lens_interfaces);
        glDeleteBuffers(1, &ssbo_ghost_data);
        glDeleteBuffers(1, &ssbo_vertex_data);
        glDeleteBuffers(1, &ubo_globals);
        
        glDeleteVertexArrays(1, &vao_quad);
        glDeleteBuffers(1, &vbo_quad);
        glDeleteBuffers(1, &ebo_quad);
    }
    
    void createShaders() {
        // Lens flare compute shader
        std::string lens_compute_source = R"(
#version 430

layout(local_size_x = 16, local_size_y = 16) in;

// Simple placeholder compute shader
void main() {
    // Minimal implementation for now
}
)";
        
        // Aperture fragment shader
        std::string aperture_fragment_source = R"(
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform float aperture_opening;
uniform float number_of_blades;
uniform float time;

void main() {
    vec2 ndc = (uv - 0.5) * 2.0;
    float dist = length(ndc);
    
    // Simple circular aperture
    float aperture_mask = smoothstep(0.8, 0.7, dist);
    
    vec3 rgb = vec3(aperture_mask);
    fragColor = vec4(rgb, 1.0);
}
)";
        
        // Simple tonemap shader
        std::string tonemap_fragment_source = R"(
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform sampler2D hdr_texture;

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr_color = texture(hdr_texture, uv).rgb;
    vec3 mapped = ACESFilm(hdr_color);
    fragColor = vec4(mapped, 1.0);
}
)";
        
        // Simple starburst shader
        std::string starburst_fragment_source = R"(
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform float time;

void main() {
    vec2 ndc = (uv - 0.5) * 2.0;
    float dist = length(ndc);
    float starburst = max(0.0, 1.0 - dist);
    
    vec3 color = vec3(1.0, 0.8, 0.6) * starburst * 0.5;
    fragColor = vec4(color, 1.0);
}
)";
        
        // Create and compile shaders
        program_lens_flare = createComputeProgram(lens_compute_source);
        program_aperture = createShaderProgram(getVertexShaderSource(), aperture_fragment_source);
        program_tonemap = createShaderProgram(getVertexShaderSource(), tonemap_fragment_source);
        program_starburst = createShaderProgram(getVertexShaderSource(), starburst_fragment_source);
    }
    
    void setupTextures() {
        // Create HDR render target
        glGenTextures(1, &texture_hdr);
        glBindTexture(GL_TEXTURE_2D, texture_hdr);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Create aperture texture
        glGenTextures(1, &texture_aperture);
        glBindTexture(GL_TEXTURE_2D, texture_aperture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, aperture_resolution, aperture_resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Create starburst texture
        glGenTextures(1, &texture_starburst);
        glBindTexture(GL_TEXTURE_2D, texture_starburst);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, starburst_resolution, starburst_resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Create framebuffers
        glGenFramebuffers(1, &fbo_hdr);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_hdr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_hdr, 0);
        
        glGenFramebuffers(1, &fbo_aperture);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_aperture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_aperture, 0);
        
        glGenFramebuffers(1, &fbo_starburst);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_starburst);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_starburst, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void setupBuffers() {
        // Create uniform buffer for global data
        glGenBuffers(1, &ubo_globals);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_globals);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalUniforms), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_globals);
        
        // Create SSBO for lens interfaces
        glGenBuffers(1, &ssbo_lens_interfaces);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_lens_interfaces);
        glBufferData(GL_SHADER_STORAGE_BUFFER, lens_interfaces.size() * sizeof(LensInterface), lens_interfaces.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lens_interfaces);
        
        // Create SSBO for ghost data
        glGenBuffers(1, &ssbo_ghost_data);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_ghost_data);
        glBufferData(GL_SHADER_STORAGE_BUFFER, ghost_data.size() * sizeof(GhostData), ghost_data.data(), GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_ghost_data);
        
        // Create SSBO for vertex data (ray tracing results)
        int total_vertices = num_ghosts * patch_tessellation * patch_tessellation;
        glGenBuffers(1, &ssbo_vertex_data);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_vertex_data);
        glBufferData(GL_SHADER_STORAGE_BUFFER, total_vertices * 4 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_vertex_data);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
};

// Additional shader sources
class ShaderLibrary {
public:
    static std::string getTonemapFragmentShader() {
        return R"(
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform sampler2D hdr_texture;

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr_color = texture(hdr_texture, uv).rgb;
    vec3 mapped = ACESFilm(hdr_color);
    fragColor = vec4(mapped, 1.0);
}
)";
    }
    
    static std::string getStarburstFragmentShader() {
        return R"(
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform sampler2D starburst_texture;
uniform float time;
uniform vec3 light_dir;
uniform vec2 backbuffer_size;

vec3 temperatureToColor(float temp) {
    float t = temp / 6000.0;
    vec3 color;
    color.r = clamp(1.0 + 0.1 * (t - 1.0), 0.6, 1.0);
    color.g = clamp(0.9 + 0.05 * (t - 1.0), 0.8, 1.0);
    color.b = clamp(0.8 + 0.2 * (1.0 - t), 0.5, 1.0);
    return color;
}

void main() {
    vec2 centered_uv = (uv - 0.5) * 2.0;
    
    // Project light direction to screen space
    vec3 screen_light_pos = light_dir * 0.5;
    vec2 starburst_center = screen_light_pos.xy;
    
    // Sample starburst texture with appropriate scaling
    float intensity = 1.0 - clamp(abs(light_dir.x * 9.0), 0.0, 1.0);
    
    // Add some animation
    float flicker1 = 1.0 - (sin(time * 5.0) + 1.0) * 0.025;
    float flicker2 = 1.0 - (sin(time * 1.0) + 1.0) * 0.0125;
    intensity *= flicker1 * flicker2;
    
    vec2 starburst_uv = (centered_uv - starburst_center) * 0.5 + 0.5;
    vec3 starburst = texture(starburst_texture, starburst_uv).rgb * intensity;
    
    starburst *= temperatureToColor(6000.0);
    
    fragColor = vec4(starburst, 1.0);
}
)";
    }
};

// Example usage class
class LensFlareDemo {
private:
    GLFWwindow* window;
    std::unique_ptr<LensFlareRenderer> renderer;
    
    float time = 0.0f;
    glm::vec3 light_direction = glm::vec3(0.0f, 0.0f, -1.0f);
    
public:
    bool initialize() {
        std::cout << "  Initializing GLFW..." << std::endl;
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        std::cout << "  Setting OpenGL context hints..." << std::endl;
        // Set OpenGL version
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        std::cout << "  Creating window..." << std::endl;
        // Create window
        window = glfwCreateWindow(1920, 1080, "OpenGL Lens Flare", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        std::cout << "  Setting up OpenGL context..." << std::endl;
        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);
        
        // Set callbacks
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetKeyCallback(window, keyCallback);
        
        std::cout << "  Creating renderer..." << std::endl;
        try {
            renderer = std::make_unique<LensFlareRenderer>();
        } catch (const std::exception& e) {
            std::cerr << "Failed to create renderer: " << e.what() << std::endl;
            return false;
        }
        
        std::cout << "  Demo initialization complete!" << std::endl;
        return true;
    }
    
    void run() {
        std::cout << "  Entering main render loop..." << std::endl;
        int frame_count = 0;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            time += 0.016f; // Assume 60 FPS
            
            if (frame_count % 60 == 0) {  // Print every 60 frames (approximately 1 second)
                std::cout << "  Frame " << frame_count << ", time: " << time << std::endl;
            }
            
            try {
                renderer->render(time, light_direction);
            } catch (const std::exception& e) {
                std::cerr << "Render error: " << e.what() << std::endl;
                break;
            }
            
            glfwSwapBuffers(window);
            frame_count++;
        }
        std::cout << "  Exited main render loop after " << frame_count << " frames." << std::endl;
    }
    
    void cleanup() {
        renderer.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        LensFlareDemo* demo = static_cast<LensFlareDemo*>(glfwGetWindowUserPointer(window));
        
        // Convert mouse position to normalized coordinates
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        float nx = (xpos / width) * 2.0f - 1.0f;
        float ny = (ypos / height) * 2.0f - 1.0f;
        
        // Update light direction based on mouse position
        demo->light_direction.x = nx * 0.2f;
        demo->light_direction.y = -ny * 0.2f;
        demo->light_direction.z = -1.0f;
        demo->light_direction = glm::normalize(demo->light_direction);
    }
    
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
};

// Main function
int main() {
    std::cout << "Starting Lens Flare Demo..." << std::endl;
    
    LensFlareDemo demo;
    
    std::cout << "Initializing demo..." << std::endl;
    if (!demo.initialize()) {
        std::cerr << "Demo initialization failed!" << std::endl;
        return -1;
    }
    
    std::cout << "Running demo..." << std::endl;
    demo.run();
    
    std::cout << "Cleaning up..." << std::endl;
    demo.cleanup();
    
    std::cout << "Demo finished successfully!" << std::endl;
    return 0;
}