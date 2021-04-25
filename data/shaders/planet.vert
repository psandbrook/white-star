#version 460 core

layout (location = 0) in vec3 pos;

out vec3 vert_pos;

layout (std140) uniform ViewProjection {
    mat4 vp;
};

void main() {
    vert_pos = pos;
    gl_Position = vp * vec4(pos, 1.0f);
}
