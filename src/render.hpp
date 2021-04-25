#pragma once

#include <utility.hpp>

#include <glad/glad.h>

#include <initializer_list>

struct GLBuffer {
public:
    u32 id = 0;
    GLenum type;
    GLenum usage;
    size_t size = 0;

    void bind();
    void buffer_data(const void* data, size_t size);
    void buffer_data_realloc(const void* data, size_t size);
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

struct ShaderProgram {
    u32 id;
    std::unordered_map<const char*, i32, CStrHash, CStrEqual> uniform_locations;

    ShaderProgram() = default;
    ShaderProgram(u32 vertex_shader, u32 fragment_shader);

    void set_uniform_f32(const char* name, f32 value);
    void set_uniform_i32(const char* name, i32 value);
    void set_uniform_bool(const char* name, bool value);
    void set_uniform_mat4(const char* name, const f32* data);
    void set_uniform_vec3(const char* name, const f32* data);
    void set_uniform_vec3(const char* name, const glm::vec3& data);

    i32 get_location(const char* name);
    void bind_uniform_block(const UniformBufferObject& ubo);
    void use();
};

struct VertexSpec {
    u32 index;
    i32 size;
    GLenum type;
    GLsizei stride;
    ptrdiff_t offset;
};

using VboMap = std::unordered_map<u32, VertexBufferObject>;

struct VertexArrayObject {
    u32 id = 0;
    ShaderProgram* shader_program;
    std::vector<u32> vbo_ids;
    ElementBufferObject ebo;

    VertexArrayObject() = default;
    VertexArrayObject(VboMap& vbos, ShaderProgram* shader_program, std::initializer_list<u32> vbo_ids,
                      std::initializer_list<VertexSpec> specs, ElementBufferObject ebo);

    VertexBufferObject& get_vbo(VboMap& vbos, u32 index);
    void draw();
    void destroy();
};

struct App;
struct Path;

struct Renderer {
    App* app;

    u32 next_vbo_id = 0;
    VboMap vbos;

    UniformBufferObject view_projection_ubo;

    ShaderProgram planet_prog;
    VertexArrayObject planet_vao;

    void init(App* app);

    void render();
    u32 compile_shader(const Path& relative_shader_path, GLenum type);
    u32 add_vbo(GLenum usage);
    void erase_vbo(u32 id);
};
