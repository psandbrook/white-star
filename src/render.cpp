#include "render.hpp"

#include "app.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <mapbox/earcut.hpp>
#include <meshoptimizer.h>

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

GLBuffer::GLBuffer(const GLenum type, const GLenum usage) : type(type), usage(usage) {
    glGenBuffers(1, &id);
    bind();
}

void GLBuffer::bind() {
    glBindBuffer(type, id);
}

void GLBuffer::buffer_data(const void* const data, const GLsizeiptr size) {
    CHECK_F(usage != GL_STATIC_DRAW);
    bind();
    if (this->size < size) {
        glBufferData(type, size, data, usage);
        this->size = size;
    } else {
        glBufferSubData(type, 0, size, data);
    }
}

void GLBuffer::buffer_data_realloc(const void* const data, const GLsizeiptr size) {
    bind();
    glBufferData(type, size, data, usage);
    this->size = size;
}

void GLBuffer::destroy() {
    glDeleteBuffers(1, &id);
    *this = GLBuffer();
}

VertexBufferObject::VertexBufferObject(const GLenum usage) : GLBuffer(GL_ARRAY_BUFFER, usage) {}

ElementBufferObject::ElementBufferObject(const GLenum usage, const GLenum primitive)
        : GLBuffer(GL_ELEMENT_ARRAY_BUFFER, usage), primitive(primitive) {}

void ElementBufferObject::buffer_elements(const void* const data, const i32 count) {
    buffer_data(data, static_cast<GLsizeiptr>(static_cast<size_t>(count) * sizeof(u32)));
    this->count = count;
}

void ElementBufferObject::buffer_elements_realloc(const void* const data, const i32 count) {
    buffer_data_realloc(data, static_cast<GLsizeiptr>(static_cast<size_t>(count) * sizeof(u32)));
    this->count = count;
}

UniformBufferObject::UniformBufferObject(const char* const name, const u32 binding, const GLenum usage)
        : GLBuffer(GL_UNIFORM_BUFFER, usage), name(name), binding(binding) {

    glBindBufferBase(GL_UNIFORM_BUFFER, binding, id);
}

Shader::Shader(const Path& shader_path, const GLenum type) : id(glCreateShader(type)) {
    auto resource_path = Path("shaders/");
    resource_path /= shader_path;
    path = app->get_resource_path(resource_path);
    load();
}

bool Shader::load() {
    auto new_time = fs::last_write_time(path);
    if (new_time > last_time) {

        const std::string source = read_file(path);
        const char* const source_c = source.c_str();

        glShaderSource(id, 1, &source_c, nullptr);
        glCompileShader(id);

        i32 success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (!success) {
            i32 len;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len));
            glGetShaderInfoLog(id, len, NULL, log.data());
            LOG_F(ERROR, "Shader compilation failed: {}", log.data());
        }

        last_time = new_time;
        return true;
    } else {
        return false;
    }
}

ShaderProgram::ShaderProgram(const Shader& vertex_shader, const Shader& fragment_shader) {
    id = glCreateProgram();
    glAttachShader(id, vertex_shader.id);
    glAttachShader(id, fragment_shader.id);
    load();
}

void ShaderProgram::load() {
    glLinkProgram(id);
    i32 success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        i32 len;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(id, len, NULL, log.data());
        LOG_F(ERROR, "Program linking failed: {}", log.data());
    }
}

i32 ShaderProgram::get_location(const char* const name) {
    i32 result = glGetUniformLocation(id, name);
    CHECK_F(result != -1);
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
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, render_samples, GL_SRGB8, static_cast<GLsizei>(width),
                                     static_cast<GLsizei>(height));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rbo);

    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, render_samples, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width),
                                     static_cast<GLsizei>(height));
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

VertexArrayObject::VertexArrayObject(const u32 shader_program_id, std::initializer_list<u32> _vbo_ids,
                                     std::initializer_list<VertexSpec> specs, const ElementBufferObject _ebo)
        : shader_program_id(shader_program_id), vbo_ids(_vbo_ids.begin(), _vbo_ids.end()), ebo(_ebo) {

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
    return app->renderer.vbos.at(vbo_ids.at(index));
}

void VertexArrayObject::draw() {
    app->renderer.shader_programs.at(shader_program_id).use();
    glBindVertexArray(id);
    glDrawElements(ebo.primitive, ebo.count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void VertexArrayObject::destroy() {
    glDeleteVertexArrays(1, &id);
    ebo.destroy();
    *this = VertexArrayObject();
}

void Renderer::init() {

    CHECK_F(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)));
    glfwSwapInterval(1);

    constexpr f32 bg_shade = 0.0f;
    glClearColor(bg_shade, bg_shade, bg_shade, 1.0f);

    glDisable(GL_DITHER);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);

    view_projection_ubo = UniformBufferObject("ViewProjection", 0, GL_STREAM_DRAW);

    std::vector<glm::vec3> vertices;
    std::vector<u32> tri_indices;
    std::vector<u32> line_indices;
    {
        using Point = std::array<f64, 2>;
        using Polygon = std::vector<std::vector<Point>>;
        Polygon polygon_vec;

        for (auto& feature : app->admin_1_fixed_l) {
            CHECK_F(feature->GetGeomFieldCount() == 1);

            OGRGeometry* geom = feature->GetGeometryRef();
            CHECK_F(geom->getGeometryType() == wkbMultiPolygon);

            OGRMultiPolygon* multi_poly = geom->toMultiPolygon();
            CHECK_F(multi_poly->getNumGeometries() > 0);

            for (auto& poly : multi_poly) {
                CHECK_NOTNULL_F(poly->getExteriorRing());
                polygon_vec.clear();

                for (auto& ring : poly) {
                    CHECK_F(ring->getNumPoints() > 0);

                    std::vector<Point> ring_vec;
                    for (auto& point : ring) {
                        CHECK_F(!point.Is3D());
                        const f64 longitude = point.getX();
                        const f64 latitude = point.getY();
                        CHECK_F(latitude >= -90 && latitude <= 90);
                        CHECK_F(longitude >= -180 && longitude <= 180);
                        ring_vec.push_back({longitude, latitude});
                    }
                    polygon_vec.push_back(std::move(ring_vec));
                }

                std::vector<u32> poly_tri_indices = mapbox::earcut<u32>(polygon_vec);
                CHECK_F(poly_tri_indices.size() % 3 == 0);

                u32 tri_vertices_offset = static_cast<u32>(vertices.size());

                for (const auto& ring : polygon_vec) {
                    u32 line_vertices_offset = static_cast<u32>(vertices.size());
                    bool first_vertex = true;

                    for (const auto& point : ring) {
                        const f64 longitude = point[0];
                        const f64 latitude = point[1];
                        const f64 azimuth = glm::radians(-longitude + 180);
                        const f64 inclination = glm::radians(-latitude + 90);
                        const glm::dvec3 v = {std::sin(inclination) * std::cos(azimuth), std::cos(inclination),
                                              std::sin(inclination) * std::sin(azimuth)};

                        u32 i = static_cast<u32>(vertices.size());
                        if (first_vertex) {
                            line_indices.push_back(i);
                            first_vertex = false;
                        } else {
                            line_indices.push_back(i);
                            line_indices.push_back(i);
                        }
                        vertices.push_back(glm::vec3(v));
                    }
                    line_indices.push_back(line_vertices_offset);
                }

                for (u32 index : poly_tri_indices) {
                    tri_indices.push_back(index + tri_vertices_offset);
                }
            }
        }
    }

    CHECK_F(tri_indices.size() % 3 == 0);
    CHECK_F(line_indices.size() % 2 == 0);
    DEXPR(vertices.size());
    DEXPR(tri_indices.size() / 3);
    DEXPR(line_indices.size() / 2);

    u32 planet_vert = add_shader("planet.vert", GL_VERTEX_SHADER);
    u32 planet_frag = add_shader("planet.frag", GL_FRAGMENT_SHADER);
    u32 planet_prog = add_shader_program(planet_vert, planet_frag);

    u32 outline_frag = add_shader("outline.frag", GL_FRAGMENT_SHADER);
    u32 outline_prog = add_shader_program(planet_vert, outline_frag);

    const u32 planet_vbo = add_vbo(GL_STATIC_DRAW);
    const VertexSpec planet_spec = {
            .index = 0,
            .size = 3,
            .type = GL_FLOAT,
            .stride = 3 * sizeof(f32),
            .offset = 0,
    };

    vbos.at(planet_vbo)
            .buffer_data_realloc(vertices.data(), static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3)));

    auto planet_ebo = ElementBufferObject(GL_STATIC_DRAW, GL_TRIANGLES);
    planet_ebo.buffer_elements_realloc(tri_indices.data(), static_cast<i32>(tri_indices.size()));
    planet_vao = VertexArrayObject(planet_prog, {planet_vbo}, {planet_spec}, planet_ebo);

    auto outline_ebo = ElementBufferObject(GL_STATIC_DRAW, GL_LINES);
    outline_ebo.buffer_elements_realloc(line_indices.data(), static_cast<i32>(line_indices.size()));
    outline_vao = VertexArrayObject(outline_prog, {planet_vbo}, {planet_spec}, outline_ebo);

    for (u32 id : {planet_prog, outline_prog}) {
        auto& program = shader_programs.at(id);
        program.bind_uniform_block(view_projection_ubo);
    }
}

void Renderer::render() {

    {
        std::unordered_set<u32> programs_to_load;
        for (auto& entry : shaders) {
            Shader& shader = entry.second;
            if (shader.load()) {
                auto range = shader_users.equal_range(shader.id);
                for (auto it = range.first; it != range.second; ++it) {
                    programs_to_load.insert(it->second);
                }
            }
        }

        for (auto id : programs_to_load) {
            shader_programs.at(id).load();
        }
    }

    if (app->wireframe_render) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, app->framebuffer_width, app->framebuffer_height);

    const glm::mat4 view = glm::lookAt(app->camera_pos, app->camera_target, app->camera_up);
    const glm::mat4 projection = glm::perspective(
            app->fovy, static_cast<f32>(app->framebuffer_width) / static_cast<f32>(app->framebuffer_height), 0.01f,
            1000.0f);

    const glm::mat4 vp = projection * view;
    view_projection_ubo.buffer_data(glm::value_ptr(vp), sizeof(vp));

    planet_vao.draw();
    outline_vao.draw();

    glfwSwapBuffers(app->window);

#ifdef DEBUG
    switch (glGetError()) {
    case GL_NO_ERROR:
        break;

    case GL_INVALID_ENUM:
        LOG_F(ERROR, "GL_INVALID_ENUM");
        break;

    case GL_INVALID_VALUE:
        LOG_F(ERROR, "GL_INVALID_VALUE");
        break;

    case GL_INVALID_OPERATION:
        LOG_F(ERROR, "GL_INVALID_OPERATION");
        break;

    case GL_INVALID_FRAMEBUFFER_OPERATION:
        LOG_F(ERROR, "GL_INVALID_FRAMEBUFFER_OPERATION");
        break;

    case GL_OUT_OF_MEMORY:
        LOG_F(ERROR, "GL_OUT_OF_MEMORY");
        break;

    default:
        LOG_F(ERROR, "Unknown OpenGL error");
        break;
    }
#endif
}

u32 Renderer::add_vbo(const GLenum usage) {
    auto vbo = VertexBufferObject(usage);
    u32 id = vbo.id;
    vbos.emplace(id, vbo);
    return id;
}

void Renderer::erase_vbo(const u32 id) {
    vbos.at(id).destroy();
    vbos.erase(id);
}

u32 Renderer::add_shader(const Path& shader_path, GLenum type) {
    auto shader = Shader(shader_path, type);
    u32 id = shader.id;
    shaders.emplace(id, shader);
    return id;
}

u32 Renderer::add_shader_program(u32 vertex_shader, u32 fragment_shader) {
    auto program = ShaderProgram(shaders.at(vertex_shader), shaders.at(fragment_shader));
    u32 id = program.id;
    shader_programs.emplace(id, program);
    shader_users.emplace(vertex_shader, id);
    shader_users.emplace(fragment_shader, id);
    return id;
}
