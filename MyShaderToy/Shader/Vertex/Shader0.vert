#version 450
#extension GL_KHR_vulkan_glsl : enable

void main() {
    vec2 pos = vec2(
        (gl_VertexIndex == 1) ? 3.0 : -1.0,
        (gl_VertexIndex == 2) ? -3.0 : 1.0
    );

    gl_Position = vec4(pos, 0.0, 1.0);
}