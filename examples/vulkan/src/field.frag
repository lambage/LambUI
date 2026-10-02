#version 450
layout(push_constant) uniform Field { vec4 bounds; vec2 viewport; float time; float strength; } field;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outputColor;
void main() {
    vec2 point = (uv - 0.5) * vec2(field.bounds.z / max(field.bounds.w, 1.0), 1.0) * 5.0;
    float height = sin(point.x * 2.7 + field.time * 0.8)
        + cos(point.y * 3.1 - field.time * 0.6)
        + sin(length(point + vec2(sin(field.time * 0.4), cos(field.time * 0.3))) * 4.0) * field.strength;
    float bands = height * 2.0;
    float distanceToLine = abs(fract(bands) - 0.5);
    float line = 1.0 - smoothstep(0.04, 0.04 + max(fwidth(bands), 0.002), distanceToLine);
    vec3 ink = mix(vec3(0.19, 0.78, 0.73), vec3(1.0, 0.43, 0.39), smoothstep(-1.8, 1.8, height));
    vec3 background = vec3(0.055, 0.075, 0.075) + ink * 0.08;
    outputColor = vec4(mix(background, ink, line), 1.0);
}