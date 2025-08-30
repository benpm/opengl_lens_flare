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