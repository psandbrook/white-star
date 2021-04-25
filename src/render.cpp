#include <render.hpp>

#include <app.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <mapbox/earcut.hpp>
#include <ogrsf_frmts.h>

#include <fstream>
#include <sstream>

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

VertexArrayObject::VertexArrayObject(VboMap& vbos, ShaderProgram* const shader_program,
                                     std::initializer_list<u32> _vbo_ids, std::initializer_list<VertexSpec> specs,
                                     const ElementBufferObject ebo_)
        : shader_program(shader_program), vbo_ids(_vbo_ids.begin(), _vbo_ids.end()), ebo(ebo_) {

    CHECK_F(vbo_ids.size() == specs.size());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &id);
    glBindVertexArray(id);

    for (u32 i = 0; i < vbo_ids.size(); ++i) {
        auto& vbo = get_vbo(vbos, i);
        const VertexSpec spec = specs.begin()[i];
        vbo.bind();
        glVertexAttribPointer(spec.index, spec.size, spec.type, false, spec.stride,
                              reinterpret_cast<void*>(spec.offset));
        glEnableVertexAttribArray(spec.index);
    }

    ebo.bind();

    glBindVertexArray(0);
}

VertexBufferObject& VertexArrayObject::get_vbo(VboMap& vbos, const u32 index) {
    return vbos.at(vbo_ids[index]);
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

void Renderer::init(App* app) {

    this->app = app;

    CHECK_F(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)));
    glfwSwapInterval(1);

    constexpr f32 bg_shade = 0.0f;
    glClearColor(bg_shade, bg_shade, bg_shade, 1.0f);

    glDisable(GL_DITHER);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_CULL_FACE);

    view_projection_ubo = UniformBufferObject("ViewProjection", 0, GL_STREAM_DRAW);

    {
        const char* const allowed_drivers_gpkg[] = {"GPKG", nullptr};
        GDALDataset* admin_1_fixed_ds = static_cast<GDALDataset*>(
                GDALOpenEx(app->get_resource_path("gis/vector/admin_1_fixed.gpkg").c_str(),
                           GDAL_OF_VECTOR | GDAL_OF_READONLY, allowed_drivers_gpkg, nullptr, nullptr));
        CHECK_NOTNULL_F(admin_1_fixed_ds);
        DEFER([&] { GDALClose(admin_1_fixed_ds); });

        OGRLayer* admin_1_fixed_l = admin_1_fixed_ds->GetLayerByName("admin_1_fixed");
        // OGRSpatialReference* spatial_ref = admin_1_fixed_l->GetSpatialRef();

        using Point = std::array<f64, 2>;
        using Polygon = std::vector<std::vector<Point>>;
        std::vector<Point> vertices_2d;
        std::vector<u32> tri_indices;

        for (auto& feature : admin_1_fixed_l) {
            CHECK_F(feature->GetGeomFieldCount() == 1);
            OGRGeometry* geom = feature->GetGeometryRef();
            CHECK_F(geom->getGeometryType() == wkbMultiPolygon);
            OGRMultiPolygon* multi_poly = geom->toMultiPolygon();
            CHECK_F(multi_poly->getNumGeometries() > 0);

            for (auto& poly : multi_poly) {
                CHECK_NOTNULL_F(poly->getExteriorRing());

                Polygon polygon_vec;
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

                std::vector<u32> poly_tri_indices = mapbox::earcut(polygon_vec);
                u32 vertices_2d_offset = static_cast<u32>(vertices_2d.size());
                for (const auto& ring : polygon_vec) {
                    for (const auto& point : ring) {
                        vertices_2d.push_back(point);
                    }
                }

                for (u32 index : poly_tri_indices) {
                    tri_indices.push_back(index + vertices_2d_offset);
                }
            }
        }

        std::vector<glm::vec3> vertices;
        for (const auto& point : vertices_2d) {
            const f64 longitude = point[0];
            const f64 latitude = point[1];
            const f64 azimuth = glm::radians(-longitude + 180);
            const f64 inclination = glm::radians(-latitude + 90);
            const glm::dvec3 v = {std::sin(inclination) * std::cos(azimuth), std::cos(inclination),
                                  std::sin(inclination) * std::sin(azimuth)};
            vertices.push_back(glm::vec3(v));
        }

        const u32 vert_shader = compile_shader("planet.vert", GL_VERTEX_SHADER);
        const u32 frag_shader = compile_shader("planet.frag", GL_FRAGMENT_SHADER);
        planet_prog = ShaderProgram(vert_shader, frag_shader);

        const u32 vbo = add_vbo(GL_STATIC_DRAW);
        const VertexSpec spec = {
                .index = 0,
                .size = 3,
                .type = GL_FLOAT,
                .stride = 3 * sizeof(f32),
                .offset = 0,
        };

        vbos.at(vbo).buffer_data_realloc(vertices.data(), vertices.size() * sizeof(glm::vec3));

        auto ebo = ElementBufferObject(GL_STATIC_DRAW, GL_TRIANGLES);
        ebo.buffer_elements_realloc(tri_indices.data(), static_cast<i32>(tri_indices.size()));

        planet_vao = VertexArrayObject(vbos, &planet_prog, {vbo}, {spec}, ebo);
    }

    for (ShaderProgram* program : {&planet_prog}) {
        program->bind_uniform_block(view_projection_ubo);
    }
}

void Renderer::render() {

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

    glfwSwapBuffers(app->window);

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

u32 Renderer::compile_shader(const Path& relative_shader_path, const GLenum type) {
    auto relative_path = Path("shaders/");
    relative_path /= relative_shader_path;
    const Path path = app->get_resource_path(relative_path);

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
