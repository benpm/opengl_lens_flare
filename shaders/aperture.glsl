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