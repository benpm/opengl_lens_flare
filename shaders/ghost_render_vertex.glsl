#version 430 core

// Input from SSBO (vertex data computed by ray tracing)
layout(std430, binding = 2) readonly buffer VertexDataBuffer {
    vec4 vertex_data[]; // x, y, intensity, w
};

uniform float ghost_id;
uniform int patch_tessellation;

out float intensity;
out vec2 aperture_coord;

void main() {
    int total_vertices_per_ghost = patch_tessellation * patch_tessellation;
    
    // For each triangle (6 vertices per quad), map to the underlying grid
    int triangle_id = gl_VertexID / 6;  // Which quad triangle are we in
    int vertex_in_triangle = gl_VertexID % 6;  // Which vertex of the triangle
    
    // Map triangle to grid coordinates
    int grid_x = triangle_id % (patch_tessellation - 1);
    int grid_y = triangle_id / (patch_tessellation - 1);
    
    // Define the quad vertices (2 triangles)
    ivec2 quad_offsets[6] = ivec2[](
        ivec2(0, 0), ivec2(1, 0), ivec2(0, 1),  // First triangle
        ivec2(1, 0), ivec2(1, 1), ivec2(0, 1)   // Second triangle
    );
    
    ivec2 offset = quad_offsets[vertex_in_triangle];
    int local_x = grid_x + offset.x;
    int local_y = grid_y + offset.y;
    
    // Clamp to valid range
    local_x = min(local_x, patch_tessellation - 1);
    local_y = min(local_y, patch_tessellation - 1);
    
    int vertex_in_ghost = local_y * patch_tessellation + local_x;
    int vertex_idx = int(ghost_id) * total_vertices_per_ghost + vertex_in_ghost;
    
    if (vertex_idx >= vertex_data.length()) {
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0); // Cull this vertex
        intensity = 0.0;
        aperture_coord = vec2(0.0);
        return;
    }
    
    vec4 vertex = vertex_data[vertex_idx];
    
    // Convert from compute shader screen space to clip space
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
    intensity = vertex.z;
    
    // Calculate aperture coordinate for texture lookup
    aperture_coord = (vec2(local_x, local_y) / float(patch_tessellation - 1) - 0.5) * 2.0;
}