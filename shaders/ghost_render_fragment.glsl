#version 430 core

in float intensity;
in vec2 aperture_coord;

uniform sampler2D aperture_texture;
uniform vec3 ghost_color;
uniform float time;

out vec4 fragColor;

vec3 temperatureToColor(float temp) {
    float t = temp / 6000.0;
    vec3 color;
    color.r = clamp(1.0 + 0.1 * (t - 1.0), 0.6, 1.0);
    color.g = clamp(0.9 + 0.05 * (t - 1.0), 0.8, 1.0);
    color.b = clamp(0.8 + 0.2 * (1.0 - t), 0.5, 1.0);
    return color;
}

void main() {
    if (intensity <= 0.0) {
        discard;
    }
    
    // Sample aperture texture
    vec2 aperture_uv = aperture_coord * 0.5 + 0.5;
    float aperture_mask = texture(aperture_texture, aperture_uv).r;
    
    if (aperture_mask < 0.01) {
        discard;
    }
    
    // Apply physically-based coloring
    vec3 base_color = temperatureToColor(6000.0);
    vec3 final_color = base_color * ghost_color * intensity * aperture_mask;
    
    // Add some subtle animation
    float flicker = 1.0 - (sin(time * 3.0) + 1.0) * 0.02;
    final_color *= flicker;
    
    fragColor = vec4(final_color, 1.0);
}