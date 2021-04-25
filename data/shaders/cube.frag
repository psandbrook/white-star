#version 460 core

in vec3 vert_pos;

out vec4 out_color;

void main() {
    out_color = vec4(vert_pos, 1.0f);
}
