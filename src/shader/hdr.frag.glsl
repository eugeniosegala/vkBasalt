#version 450
layout(location = 0) in vec2 textureCoord;
layout(location = 0) out vec4 outputColor;
layout(set = 0, binding = 0) uniform sampler2D sourceImage;
layout(constant_id = 0) const int direction = 0;
const float m1 = 2610.0 / 16384.0;
const float m2 = 2523.0 / 32.0;
const float c1 = 3424.0 / 4096.0;
const float c2 = 2413.0 / 128.0;
const float c3 = 2392.0 / 128.0;
void main() {
    vec4 color = texture(sourceImage, textureCoord);
    if (direction == 1) {
        const mat3 to2020 = mat3(0.627404, 0.069097, 0.016391,
            0.329283, 0.919540, 0.088013, 0.043313, 0.011362, 0.895595);
        vec3 p = pow(clamp(to2020 * color.rgb / 125.0, 0.0, 1.0), vec3(m1));
        color.rgb = pow((vec3(c1) + c2 * p) / (vec3(1.0) + c3 * p), vec3(m2));
    } else if (direction == 2) {
        vec3 p = pow(clamp(color.rgb, 0.0, 1.0), vec3(1.0 / m2));
        vec3 linear2020 = pow(max(p - c1, 0.0) / max(c2 - c3 * p, 1e-6), vec3(1.0 / m1));
        const mat3 to709 = mat3(1.660491, -0.124550, -0.018151,
            -0.587641, 1.132900, -0.100579, -0.072850, -0.008349, 1.118730);
        color.rgb = to709 * linear2020 * 125.0;
    }
    outputColor = color;
}
