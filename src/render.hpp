#pragma once

#include "filesystem.hpp"
#include "utility.hpp"

#include <glad/glad.h>
#include <glm/vec3.hpp>

#include <initializer_list>
#include <unordered_map>

struct GLBuffer {
public:
    u32 id = 0;
    GLenum type;
    GLenum usage;
    GLsizeiptr size = 0;

    void bind();
    void buffer_data(const void* data, GLsizeiptr size);
    void buffer_data_realloc(const void* data, GLsizeiptr size);
    void destroy();

protected:
    GLBuffer() = default;
    GLBuffer(GLenum type, GLenum usage);
};

struct VertexBufferObject : public GLBuffer {
    VertexBufferObject() = default;
    explicit VertexBufferObject(GLenum usage);
};

struct ElementBufferObject : public GLBuffer {
    GLenum primitive;
    i32 count = 0;

    ElementBufferObject() = default;
    ElementBufferObject(GLenum usage, GLenum primitive);

    void buffer_elements(const void* data, i32 count);
    void buffer_elements_realloc(const void* data, i32 count);
};

struct UniformBufferObject : public GLBuffer {
    const char* name;
    u32 binding;

    UniformBufferObject() = default;
    UniformBufferObject(const char* name, u32 binding, GLenum usage);
};

struct Framebuffer {
    static void bind_default();

    u32 id = 0;
    u32 width, height;

    union {
        struct {
            u32 color_rbo;
            u32 depth_rbo;
        };
        u32 rbos[2] = {};
    };

    Framebuffer() = default;
    Framebuffer(u32 width, u32 height);

    void bind();
    void destroy();
};

struct Shader {
    u32 id = 0;
    Path path;
    std::filesystem::file_time_type last_time = std::filesystem::file_time_type::min();

    Shader() = default;
    Shader(const Path& shader_path, GLenum type);

    bool load();
};

struct ShaderProgram {
    u32 id = 0;

    ShaderProgram() = default;
    ShaderProgram(const Shader& vertex_shader, const Shader& fragment_shader);

    void set_uniform_f32(const char* name, f32 value);
    void set_uniform_i32(const char* name, i32 value);
    void set_uniform_bool(const char* name, bool value);
    void set_uniform_mat4(const char* name, const f32* data);
    void set_uniform_vec3(const char* name, const f32* data);
    void set_uniform_vec3(const char* name, const glm::vec3& data);

    i32 get_location(const char* name);
    void bind_uniform_block(const UniformBufferObject& ubo);
    void use();
    void load();
};

struct VertexSpec {
    u32 index;
    i32 size;
    GLenum type;
    GLsizei stride;
    ptrdiff_t offset;
};

struct VertexArrayObject {
    u32 id = 0;
    ShaderProgram* shader_program;
    std::vector<u32> vbo_ids;
    ElementBufferObject ebo;

    VertexArrayObject() = default;
    VertexArrayObject(ShaderProgram* shader_program, std::initializer_list<u32> vbo_ids,
                      std::initializer_list<VertexSpec> specs, ElementBufferObject ebo);

    VertexBufferObject& get_vbo(u32 index);
    void draw();
    void destroy();
};

struct Renderer {
    std::unordered_map<u32, VertexBufferObject> vbos;

    UniformBufferObject view_projection_ubo;

    std::unordered_multimap<u32, ShaderProgram*> shader_users;

    Shader planet_vert;
    Shader planet_frag;
    Shader outline_frag;

    ShaderProgram planet_prog;
    ShaderProgram outline_prog;

    VertexArrayObject planet_vao;
    VertexArrayObject outline_vao;

    void init();

    void render();
    u32 add_vbo(GLenum usage);
    void erase_vbo(u32 id);
};
