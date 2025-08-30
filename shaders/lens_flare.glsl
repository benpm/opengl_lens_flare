#version 330 core

in vec2 uv;
out vec4 fragColor;

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

vec3 createGhost(vec2 uv, vec2 center, float size, vec3 color, float intensity) {
    vec2 delta = uv - center;
    float dist = length(delta);
    float falloff = smoothstep(size, size * 0.5, dist);
    return color * falloff * intensity;
}

void main() {
    vec2 centered_uv = (uv - 0.5) * 2.0;
    
    // Project light direction to screen space
    vec2 light_screen_pos = light_dir.xy * 0.3;
    
    // Make the effect always visible for testing - use a high base intensity
    float base_intensity = 0.8;
    
    // Add some animation
    float flicker1 = 1.0 - (sin(time * 5.0) + 1.0) * 0.025;
    float flicker2 = 1.0 - (sin(time * 1.0) + 1.0) * 0.0125;
    base_intensity *= flicker1 * flicker2;
    
    vec3 final_color = vec3(0.0);
    
    // Always show the effect for testing
    if (true) {
        // Main bright spot
        final_color += createGhost(centered_uv, light_screen_pos, 0.1, 
                                 temperatureToColor(6000.0), base_intensity * 5.0);
        
        // Secondary ghosts at different positions
        vec2 ghost_dir = normalize(-light_screen_pos);
        
        final_color += createGhost(centered_uv, light_screen_pos + ghost_dir * 0.3, 0.05,
                                 vec3(1.0, 0.7, 0.3), base_intensity * 2.0);
        
        final_color += createGhost(centered_uv, light_screen_pos + ghost_dir * 0.6, 0.08,
                                 vec3(0.3, 0.7, 1.0), base_intensity * 1.5);
        
        final_color += createGhost(centered_uv, light_screen_pos + ghost_dir * 0.9, 0.03,
                                 vec3(1.0, 0.3, 0.7), base_intensity * 1.0);
        
        // Add some starbursts/rays
        float angle = atan(centered_uv.y - light_screen_pos.y, centered_uv.x - light_screen_pos.x);
        float ray_intensity = 0.0;
        
        // Create 6 rays
        for (int i = 0; i < 6; i++) {
            float ray_angle = float(i) * 3.14159 / 3.0;
            float angle_diff = abs(mod(angle - ray_angle + 3.14159, 2.0 * 3.14159) - 3.14159);
            ray_intensity += exp(-angle_diff * 20.0) * 0.1;
        }
        
        float dist_from_light = length(centered_uv - light_screen_pos);
        ray_intensity *= smoothstep(0.8, 0.1, dist_from_light);
        
        final_color += temperatureToColor(6000.0) * ray_intensity * base_intensity;
    }
    
    fragColor = vec4(final_color, 1.0);
}