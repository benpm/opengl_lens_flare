#version 430

layout(local_size_x = 16, local_size_y = 16) in;

layout(std140, binding = 0) uniform GlobalUniforms {
    float time;
    float spread;
    float plate_size;
    float aperture_id;
    float num_interfaces;
    float coating_quality;
    vec2 backbuffer_size;
    vec3 light_dir;
    float aperture_resolution;
    float aperture_opening;
    float number_of_blades;
    float starburst_resolution;
    float padding;
};

struct LensInterface {
    vec3 center;
    float radius;
    vec3 n; // n.x = left IOR, n.y = coating IOR, n.z = right IOR
    float sa; // surface aperture
    float d1; // coating thickness
    float is_flat; // flat surface flag (renamed to avoid keyword conflict)
    float pos; // position along optical axis
    float w; // width factor
};

struct GhostData {
    float bounce1;
    float bounce2;
    float padding1;
    float padding2;
};

layout(std430, binding = 0) readonly buffer LensInterfaceBuffer {
    LensInterface lens_interfaces[];
};

layout(std430, binding = 1) readonly buffer GhostDataBuffer {
    GhostData ghost_data[];
};

layout(std430, binding = 2) writeonly buffer VertexDataBuffer {
    vec4 vertex_data[];
};

uniform sampler2D aperture_texture;

// Ray-sphere intersection
bool intersectSphere(vec3 rayOrigin, vec3 rayDir, vec3 sphereCenter, float sphereRadius, out float t) {
    vec3 oc = rayOrigin - sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(oc, rayDir);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) return false;
    
    float sqrt_discriminant = sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2 * a);
    float t2 = (-b + sqrt_discriminant) / (2 * a);
    
    t = (t1 > 0) ? t1 : t2;
    return t > 0;
}

// Ray-plane intersection  
bool intersectPlane(vec3 rayOrigin, vec3 rayDir, vec3 planeCenter, vec3 planeNormal, out float t) {
    float denom = dot(planeNormal, rayDir);
    if (abs(denom) < 1e-6) return false;
    
    vec3 p0l0 = planeCenter - rayOrigin;
    t = dot(p0l0, planeNormal) / denom;
    return t >= 0;
}

// Fresnel reflectance
float fresnel(float cosTheta, float n1, float n2) {
    float r0 = (n1 - n2) / (n1 + n2);
    r0 = r0 * r0;
    float cosX = 1.0 - cosTheta;
    return r0 + (1.0 - r0) * pow(cosX, 5.0);
}

void main() {
    uvec3 thread_id = gl_GlobalInvocationID;
    uint ghost_id = thread_id.x / 16; // Each group handles one ghost
    
    if (ghost_id >= ghost_data.length()) return;
    
    // Get local coordinates within the patch
    uint local_x = thread_id.x % 16;
    uint local_y = thread_id.y;
    
    if (local_x >= 16 || local_y >= 16) return;
    
    GhostData ghost = ghost_data[ghost_id];
    uint bounce1_idx = uint(ghost.bounce1);
    uint bounce2_idx = uint(ghost.bounce2);
    
    if (bounce1_idx >= lens_interfaces.length() || bounce2_idx >= lens_interfaces.length()) return;
    
    // Generate ray from light source
    vec2 aperture_coord = (vec2(local_x, local_y) / 16.0 - 0.5) * 2.0;
    vec3 ray_origin = vec3(aperture_coord * 10.0, -100.0); // Start far behind lens
    vec3 ray_dir = normalize(light_dir);
    
    vec3 current_pos = ray_origin;
    vec3 current_dir = ray_dir;
    float intensity = 1.0;
    
    // Trace through lens system to first bounce
    for (uint i = 0; i < bounce1_idx && i < lens_interfaces.length(); ++i) {
        LensInterface iface = lens_interfaces[i];
        float t;
        bool hit = false;
        
        if (iface.is_flat > 0.5) {
            hit = intersectPlane(current_pos, current_dir, iface.center, vec3(0, 0, 1), t);
        } else {
            hit = intersectSphere(current_pos, current_dir, iface.center, iface.radius, t);
        }
        
        if (!hit) {
            intensity = 0.0;
            break;
        }
        
        current_pos += current_dir * t;
        
        // Apply refraction/reflection (simplified)
        if (i == bounce1_idx - 1) {
            // First reflection
            vec3 normal = normalize(current_pos - iface.center);
            current_dir = reflect(current_dir, normal);
            intensity *= fresnel(abs(dot(current_dir, normal)), iface.n.x, iface.n.z) * 0.1;
        }
    }
    
    // Continue to second bounce
    for (uint i = bounce1_idx; i < bounce2_idx && i < lens_interfaces.length(); ++i) {
        LensInterface iface = lens_interfaces[i];
        float t;
        bool hit = false;
        
        if (iface.is_flat > 0.5) {
            hit = intersectPlane(current_pos, current_dir, iface.center, vec3(0, 0, 1), t);
        } else {
            hit = intersectSphere(current_pos, current_dir, iface.center, iface.radius, t);
        }
        
        if (!hit) {
            intensity = 0.0;
            break;
        }
        
        current_pos += current_dir * t;
        
        // Apply refraction/reflection (simplified)
        if (i == bounce2_idx - 1) {
            // Second reflection
            vec3 normal = normalize(current_pos - iface.center);
            current_dir = reflect(current_dir, normal);
            intensity *= fresnel(abs(dot(current_dir, normal)), iface.n.x, iface.n.z) * 0.1;
        }
    }
    
    // Project final ray to screen
    vec2 screen_pos = current_pos.xy / backbuffer_size * 2.0 - 1.0;
    
    // Store vertex data
    uint vertex_idx = ghost_id * 256 + local_y * 16 + local_x;
    if (vertex_idx < vertex_data.length()) {
        vertex_data[vertex_idx] = vec4(screen_pos, intensity, 1.0);
    }
}