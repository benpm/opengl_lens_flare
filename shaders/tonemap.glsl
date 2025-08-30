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