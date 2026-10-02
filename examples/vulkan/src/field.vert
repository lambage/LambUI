#version 450
layout(push_constant) uniform Field { vec4 bounds; vec2 viewport; float time; float strength; } field;
layout(location = 0) out vec2 uv;
void main() {
    const vec2 corners[6] = vec2[6](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,0), vec2(1,1), vec2(0,1));
    uv = corners[gl_VertexIndex];
    vec2 position = field.bounds.xy + uv * field.bounds.zw;
    gl_Position = vec4(position / field.viewport * 2.0 - 1.0, 0.0, 1.0);
}