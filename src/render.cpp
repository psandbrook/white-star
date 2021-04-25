#include <render.hpp>

#include <filesystem.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <fstream>
#include <sstream>

namespace {

std::unordered_map<u32, VertexBufferObject>* vbos_ptr = nullptr;

u32 compile_shader(const Path& relative_shader_path, const GLenum type) {
    auto relative_path = Path("shaders/");
    relative_path /= relative_shader_path;
    const Path path = get_resource_path(relative_path);

    std::ifstream stream(path);
    CHECK_F(bool(stream));

    std::stringstream source_buffer;
    source_buffer << stream.rdbuf();
    const std::string source = source_buffer.str();
    const char* const source_c = source.c_str();

    const u32 shader = glCreateShader(type);
    glShaderSource(shader, 1, &source_c, nullptr);
    glCompileShader(shader);

    i32 success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        i32 len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetShaderInfoLog(shader, len, NULL, log.data());
        ABORT_F("Shader compilation failed: {}", log.data());
    }

    return shader;
}
} // namespace

GLBuffer::GLBuffer(const GLenum type, const GLenum usage) : type(type), usage(usage) {
    glGenBuffers(1, &id);
    bind();
}

void GLBuffer::bind() {
    glBindBuffer(type, id);
}

void GLBuffer::buffer_data(const void* const data, const size_t data_size) {
    CHECK_F(usage != GL_STATIC_DRAW);
    bind();
    if (this->size < data_size) {
        glBufferData(type, data_size, data, usage);
        this->size = data_size;
    } else {
        glBufferSubData(type, 0, data_size, data);
    }
}

void GLBuffer::buffer_data_realloc(const void* const data, const size_t data_size) {
    bind();
    glBufferData(type, data_size, data, usage);
    this->size = data_size;
}

void GLBuffer::destroy() {
    glDeleteBuffers(1, &id);
    *this = GLBuffer();
}

VertexBufferObject::VertexBufferObject(const GLenum usage) : GLBuffer(GL_ARRAY_BUFFER, usage) {}

ElementBufferObject::ElementBufferObject(const GLenum usage, const GLenum primitive)
        : GLBuffer(GL_ELEMENT_ARRAY_BUFFER, usage), primitive(primitive) {}

void ElementBufferObject::buffer_elements(const void* const data, const i32 count) {
    buffer_data(data, static_cast<size_t>(count) * sizeof(u32));
    this->count = count;
}

void ElementBufferObject::buffer_elements_realloc(const void* const data, const i32 count) {
    buffer_data_realloc(data, static_cast<size_t>(count) * sizeof(u32));
    this->count = count;
}

UniformBufferObject::UniformBufferObject(const char* const name, const u32 binding, const GLenum usage)
        : GLBuffer(GL_UNIFORM_BUFFER, usage), name(name), binding(binding) {

    glBindBufferBase(GL_UNIFORM_BUFFER, binding, id);
}

ShaderProgram::ShaderProgram(const u32 vertex_shader, const u32 fragment_shader) {
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);

    i32 success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        i32 len;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(id, len, NULL, log.data());
        ABORT_F("Program linking failed: {}", log.data());
    }
}

i32 ShaderProgram::get_location(const char* const name) {
    auto it = uniform_locations.find(name);
    i32 result;
    if (it == uniform_locations.end()) {
        result = glGetUniformLocation(id, name);
        CHECK_F(result != -1);
        uniform_locations.emplace(name, result);
    } else {
        result = it->second;
    }

    return result;
}

void ShaderProgram::use() {
    glUseProgram(id);
}

void ShaderProgram::set_uniform_mat4(const char* const name, const f32* const data) {
    use();
    glUniformMatrix4fv(get_location(name), 1, false, data);
}

void ShaderProgram::set_uniform_f32(const char* const name, const f32 value) {
    use();
    glUniform1f(get_location(name), value);
}

void ShaderProgram::set_uniform_i32(const char* const name, const i32 value) {
    use();
    glUniform1i(get_location(name), value);
}

void ShaderProgram::set_uniform_bool(const char* const name, const bool value) {
    set_uniform_i32(name, value);
}

void ShaderProgram::set_uniform_vec3(const char* const name, const f32* const data) {
    use();
    glUniform3fv(get_location(name), 1, data);
}

void ShaderProgram::set_uniform_vec3(const char* name, const glm::vec3& data) {
    set_uniform_vec3(name, glm::value_ptr(data));
}

void ShaderProgram::bind_uniform_block(const UniformBufferObject& ubo) {
    const u32 index = glGetUniformBlockIndex(id, ubo.name);
    glUniformBlockBinding(id, index, ubo.binding);
}

void Framebuffer::bind_default() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Framebuffer::Framebuffer(const u32 width, const u32 height) : width(width), height(height) {
    glGenFramebuffers(1, &id);
    bind();

    glGenRenderbuffers(2, rbos);
    glBindRenderbuffer(GL_RENDERBUFFER, color_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, render_samples, GL_SRGB8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rbo);

    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, render_samples, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);

#ifdef DEBUG
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
        break;

    case GL_FRAMEBUFFER_UNDEFINED:
        ABORT_F("GL_FRAMEBUFFER_UNDEFINED");

    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");

    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");

    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");

    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");

    case GL_FRAMEBUFFER_UNSUPPORTED:
        ABORT_F("GL_FRAMEBUFFER_UNSUPPORTED");

    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");

    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        ABORT_F("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS");

    default:
        ABORT_F("glCheckFramebufferStatus() failed");
    }
#endif

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    bind_default();
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void Framebuffer::destroy() {
    glDeleteFramebuffers(1, &id);
    glDeleteRenderbuffers(2, rbos);
    *this = Framebuffer();
}

VertexArrayObject::VertexArrayObject(ShaderProgram* const shader_program, std::initializer_list<u32> vbos_,
                                     std::initializer_list<VertexSpec> specs, const ElementBufferObject ebo_)
        : shader_program(shader_program), vbo_ids(vbos_.begin(), vbos_.end()), ebo(ebo_) {

    CHECK_F(vbo_ids.size() == specs.size());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    for (u32 i = 0; i < vbo_ids.size(); ++i) {
        auto& vbo = get_vbo(i);
        const VertexSpec spec = specs.begin()[i];
        vbo.bind();
        glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride,
                              reinterpret_cast<void*>(spec.offset));
        glEnableVertexAttribArray(spec.index);
    }

    ebo.bind();

    glBindVertexArray(0);
}

VertexBufferObject& VertexArrayObject::get_vbo(const u32 index) {
    return vbos_ptr->at(vbo_ids[index]);
}

void VertexArrayObject::draw() {
    shader_program->use();
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void VertexArrayObject::destroy() {
    glDeleteVertexArrays(1, &id);
    ebo.destroy();
    *this = VertexArrayObject();
}

Renderer::Renderer(AppState* const app_state) : app_state(app_state) {

    vbos_ptr = &vbos;

    const char* const allowed_drivers_gpkg[] = {"GPKG", nullptr};
    admin_1_fixed_ds = static_cast<GDALDataset*>(GDALOpenEx(get_resource_path("gis/vector/admin_1_fixed.gpkg").c_str(),
                                                            GDAL_OF_VECTOR | GDAL_OF_READONLY, allowed_drivers_gpkg,
                                                            nullptr, nullptr));
    CHECK_NOTNULL_F(admin_1_fixed_ds);

    admin_1_fixed_l = admin_1_fixed_ds->GetLayerByName("admin_1_fixed");
    DEXPR(admin_1_fixed_l->GetFeatureCount());

    glfwSwapInterval(1);

    constexpr f32 bg_shade = 0.0f;
    glClearColor(bg_shade, bg_shade, bg_shade, 1.0f);

    glDisable(GL_DITHER);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    view_projection_ubo = UniformBufferObject("ViewProjection", 0, GL_DYNAMIC_DRAW);

    {
        const u32 vert_shader = compile_shader("cube.vert", GL_VERTEX_SHADER);
        const u32 frag_shader = compile_shader("cube.frag", GL_FRAGMENT_SHADER);
        cube_prog = ShaderProgram(vert_shader, frag_shader);

        const u32 vbo = add_vbo(GL_STATIC_DRAW);
        const VertexSpec spec = {
                .index = 0,
                .size = 3,
                .type = GL_FLOAT,
                .stride = 3 * sizeof(f32),
                .offset = 0,
        };

        // clang-format off
        const f32 cube_vertices[] = {
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
        };

        const u32 cube_indices[] = {
            0, 1, 2,
            1, 3, 2,
            4, 6, 5,
            5, 6, 7,
            1, 5, 3,
            3, 5, 7,
            3, 7, 6,
            3, 6, 2,
            6, 4, 2,
            0, 2, 4,
            0, 5, 1,
            0, 4, 5,
        };
        // clang-format on

        vbos.at(vbo).buffer_data_realloc(cube_vertices, sizeof(cube_vertices));

        auto ebo = ElementBufferObject(GL_STATIC_DRAW, GL_TRIANGLES);
        ebo.buffer_elements_realloc(cube_indices, std::size(cube_indices));

        cube_vao = VertexArrayObject(&cube_prog, {vbo}, {spec}, ebo);
    }

    for (ShaderProgram* program : {&cube_prog}) {
        program->bind_uniform_block(view_projection_ubo);
    }
}

u32 Renderer::add_vbo(const GLenum usage) {
    while (has_key(vbos, next_vbo_id)) {
        ++next_vbo_id;
    }

    const u32 id = next_vbo_id;
    ++next_vbo_id;
    vbos.emplace(id, VertexBufferObject(usage));
    return id;
}

void Renderer::erase_vbo(const u32 id) {
    vbos.at(id).destroy();
    vbos.erase(id);
}

void Renderer::render() {

    if (app_state->wireframe_render) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, app_state->framebuffer_width, app_state->framebuffer_height);

    const glm::mat4 view = glm::lookAt(app_state->camera_pos, app_state->camera_target, app_state->camera_up);
    const glm::mat4 projection = glm::perspective(app_state->fovy,
                                                  static_cast<f32>(app_state->framebuffer_width) /
                                                          static_cast<f32>(app_state->framebuffer_height),
                                                  0.01f, 1000.0f);

    const glm::mat4 vp = projection * view;
    view_projection_ubo.buffer_data(glm::value_ptr(vp), sizeof(vp));

    cube_vao.draw();

    glfwSwapBuffers(app_state->window);

#ifdef DEBUG
    switch (glGetError()) {
    case GL_NO_ERROR:
        break;

    case GL_INVALID_ENUM:
        ABORT_F("GL_INVALID_ENUM");

    case GL_INVALID_VALUE:
        ABORT_F("GL_INVALID_VALUE");

    case GL_INVALID_OPERATION:
        ABORT_F("GL_INVALID_OPERATION");

    case GL_INVALID_FRAMEBUFFER_OPERATION:
        ABORT_F("GL_INVALID_FRAMEBUFFER_OPERATION");

    case GL_OUT_OF_MEMORY:
        ABORT_F("GL_OUT_OF_MEMORY");

    default:
        ABORT_F("Unknown OpenGL error");
    }
#endif
}
