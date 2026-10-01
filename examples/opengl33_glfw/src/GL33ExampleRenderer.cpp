#include "GL33ExampleRenderer.h"
#include "LambUI/UILog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

using namespace LambUI;

namespace {
constexpr const char* TAG = "GL33ExampleRenderer";

constexpr const char* VertexShader = R"glsl(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
uniform vec2 displaySize;
out vec2 uv;
void main() {
    uv = texCoord;
    gl_Position = vec4(position.x / displaySize.x * 2.0 - 1.0,
                       1.0 - position.y / displaySize.y * 2.0, 0.0, 1.0);
}
)glsl";

constexpr const char* FragmentShader = R"glsl(#version 330 core
in vec2 uv;
out vec4 fragmentColor;
uniform sampler2D image;
uniform vec4 tint;
uniform int mode;
uniform float edge;
uniform float time;
uniform float strength;
void main() {
    if (mode == 1) {
        fragmentColor = texture(image, uv) * tint;
    } else if (mode == 2) {
        float distanceValue = texture(image, uv).r;
        float smoothing = max(fwidth(distanceValue), 1.0 / 255.0);
        float coverage = smoothstep(edge - smoothing, edge + smoothing, distanceValue);
        fragmentColor = vec4(tint.rgb, tint.a * coverage);
    } else if (mode == 3) {
        vec2 point = uv * 2.0 - 1.0;
        float wave = sin(point.x * 12.0 + time) * cos(point.y * 10.0 - time * 0.7);
        float contour = abs(sin((length(point) * 14.0 + wave * strength * 3.0) - time));
        float line = 1.0 - smoothstep(0.03, 0.03 + fwidth(contour) * 1.5, contour);
        vec3 base = mix(vec3(0.035, 0.065, 0.075), vec3(0.08, 0.23, 0.22), wave * 0.5 + 0.5);
        vec3 ink = mix(vec3(0.25, 0.86, 0.64), vec3(0.95, 0.45, 0.36), uv.x);
        fragmentColor = vec4(mix(base, ink, line), 1.0) * tint;
    } else {
        fragmentColor = tint;
    }
}
)glsl";

GLuint CompileShader(GLenum type, const char* source) {
    LAMBUI_LOGT(TAG, "CompileShader({})", type);
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;
    std::array<char, 4096> message{};
    glGetShaderInfoLog(shader, static_cast<GLsizei>(message.size()), nullptr, message.data());
    LAMBUI_LOGE(TAG, "Shader compilation failed: {}", message.data());
    glDeleteShader(shader);
    return 0;
}
}

GL33ExampleRenderer::GL33ExampleRenderer() {
    LAMBUI_LOGT(TAG, "Construct");
}

GL33ExampleRenderer::~GL33ExampleRenderer() {
    LAMBUI_LOGT(TAG, "Destroy");
    for (const auto& entry : m_fonts) glDeleteTextures(1, &entry.second.second);
    glDeleteBuffers(1, &m_vertexBuffer);
    glDeleteVertexArrays(1, &m_vertexArray);
    glDeleteProgram(m_program);
}

bool GL33ExampleRenderer::Initialize() {
    LAMBUI_LOGT(TAG, "Initialize");
    const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VertexShader);
    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShader);
    if (!vertexShader || !fragmentShader) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        std::array<char, 4096> message{};
        glGetProgramInfoLog(m_program, static_cast<GLsizei>(message.size()), nullptr, message.data());
        LAMBUI_LOGE(TAG, "Shader link failed: {}", message.data());
        return false;
    }
    m_displayLocation = glGetUniformLocation(m_program, "displaySize");
    m_colorLocation = glGetUniformLocation(m_program, "tint");
    m_modeLocation = glGetUniformLocation(m_program, "mode");
    m_edgeLocation = glGetUniformLocation(m_program, "edge");
    m_timeLocation = glGetUniformLocation(m_program, "time");
    m_strengthLocation = glGetUniformLocation(m_program, "strength");
    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "image"), 0);
    glGenVertexArrays(1, &m_vertexArray);
    glGenBuffers(1, &m_vertexBuffer);
    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    return glGetError() == GL_NO_ERROR;
}

void GL33ExampleRenderer::SetViewportSize(int width, int height, int framebufferWidth, int framebufferHeight) {
    if (width == m_width && height == m_height && framebufferWidth == m_framebufferWidth &&
        framebufferHeight == m_framebufferHeight) return;
    LAMBUI_LOGT(TAG, "SetViewportSize({}, {}, {}, {})", width, height, framebufferWidth, framebufferHeight);
    m_width = width;
    m_height = height;
    m_framebufferWidth = framebufferWidth;
    m_framebufferHeight = framebufferHeight;
}

bool GL33ExampleRenderer::LoadFont(const FontAtlas& atlas, void* fontHandle) {
    LAMBUI_LOGT(TAG, "LoadFont({}, {}x{})", fmt::ptr(fontHandle), atlas.GetAtlasWidth(), atlas.GetAtlasHeight());
    if (atlas.GetAtlasPixels().empty() || m_fonts.count(fontHandle)) return false;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint unpackAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlas.GetAtlasWidth(), atlas.GetAtlasHeight(),
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas.GetAtlasPixels().data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }
    m_fonts.emplace(fontHandle, std::make_pair(&atlas, texture));
    return true;
}

void GL33ExampleRenderer::SetEffect(float time, float strength) {
    LAMBUI_LOGT(TAG, "SetEffect({}, {})", time, strength);
    m_time = time;
    m_strength = std::clamp(strength, 0.0f, 1.0f);
}

void GL33ExampleRenderer::BindPipeline() {
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_program);
    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glUniform2f(m_displayLocation, static_cast<float>(m_width), static_cast<float>(m_height));
}

void GL33ExampleRenderer::DrawQuad(const UIRenderCommand& command, int mode, GLuint texture, float edge) {
    if (m_width <= 0 || m_height <= 0 || m_framebufferWidth <= 0 || m_framebufferHeight <= 0) return;
    BindPipeline();
    const float left = command.x;
    const float top = command.y;
    const float right = left + command.width;
    const float bottom = top + command.height;
    const std::array<float, 24> vertices = {
        left, top, command.u0, command.v0, right, top, command.u1, command.v0,
        right, bottom, command.u1, command.v1, left, top, command.u0, command.v0,
        right, bottom, command.u1, command.v1, left, bottom, command.u0, command.v1
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
    glUniform4f(m_colorLocation,
                static_cast<float>((command.color >> 24) & 255) / 255.0f,
                static_cast<float>((command.color >> 16) & 255) / 255.0f,
                static_cast<float>((command.color >> 8) & 255) / 255.0f,
                static_cast<float>(command.color & 255) / 255.0f);
    glUniform1i(m_modeLocation, mode);
    glUniform1f(m_edgeLocation, edge);
    glUniform1f(m_timeLocation, m_time);
    glUniform1f(m_strengthLocation, m_strength);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GL33ExampleRenderer::DrawString(const UIRenderCommand& command) {
    auto found = m_fonts.find(command.fontHandle);
    if (found == m_fonts.end()) found = m_fonts.find(nullptr);
    if (found == m_fonts.end()) return;
    const auto& [atlas, texture] = found->second;
    float penX = command.x;
    for (unsigned char character : command.text) {
        const auto* glyph = atlas->FindGlyph(static_cast<char32_t>(character));
        if (!glyph) continue;
        if (glyph->width > 0.0f && glyph->height > 0.0f) {
            UIRenderCommand quad;
            quad.x = penX + glyph->bearingX;
            quad.y = command.y + atlas->GetAscent() + glyph->bearingY;
            quad.width = glyph->width;
            quad.height = glyph->height;
            quad.u0 = glyph->u0;
            quad.v0 = glyph->v0;
            quad.u1 = glyph->u1;
            quad.v1 = glyph->v1;
            quad.color = command.color;
            DrawQuad(quad, 2, texture, atlas->GetOnEdgeValue() / 255.0f);
        }
        penX += glyph->advance;
    }
}

void GL33ExampleRenderer::DrawEffect(const UICustomRenderArgs& args) {
    LAMBUI_LOGT(TAG, "DrawEffect({}, {}, {}, {})", args.viewportX, args.viewportY,
                args.viewportWidth, args.viewportHeight);
    UIRenderCommand quad;
    quad.x = args.viewportX;
    quad.y = args.viewportY;
    quad.width = args.viewportWidth;
    quad.height = args.viewportHeight;
    DrawQuad(quad, 3);
}

void GL33ExampleRenderer::SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) {
    LAMBUI_LOGT(TAG, "SubmitRenderCommands({})", commands.size());
    if (m_width <= 0 || m_height <= 0 || m_framebufferWidth <= 0 || m_framebufferHeight <= 0) return;
    std::vector<std::array<GLint, 4>> clips;
    const float scaleX = static_cast<float>(m_framebufferWidth) / m_width;
    const float scaleY = static_cast<float>(m_framebufferHeight) / m_height;
    const auto applyClip = [&clips]() {
        if (clips.empty()) {
            glDisable(GL_SCISSOR_TEST);
        } else {
            glEnable(GL_SCISSOR_TEST);
            const auto& clip = clips.back();
            glScissor(clip[0], clip[1], clip[2], clip[3]);
        }
    };
    applyClip();
    for (const auto& command : commands) {
        switch (command.type) {
            case RenderCommandType::DrawQuad:
                DrawQuad(command, command.textureHandle ? 1 : 0,
                         static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(command.textureHandle)));
                break;
            case RenderCommandType::DrawString:
                DrawString(command);
                break;
            case RenderCommandType::PushScissor: {
                GLint left = static_cast<GLint>(std::floor(command.x * scaleX));
                GLint bottom = m_framebufferHeight - static_cast<GLint>(std::ceil((command.y + command.height) * scaleY));
                GLint right = static_cast<GLint>(std::ceil((command.x + command.width) * scaleX));
                GLint top = m_framebufferHeight - static_cast<GLint>(std::floor(command.y * scaleY));
                const std::array<GLint, 4> parent = clips.empty()
                    ? std::array<GLint, 4>{0, 0, m_framebufferWidth, m_framebufferHeight} : clips.back();
                left = std::max(left, parent[0]);
                bottom = std::max(bottom, parent[1]);
                right = std::min(right, parent[0] + parent[2]);
                top = std::min(top, parent[1] + parent[3]);
                clips.push_back({left, bottom, std::max(0, right - left), std::max(0, top - bottom)});
                applyClip();
                break;
            }
            case RenderCommandType::PopScissor:
                if (!clips.empty()) clips.pop_back();
                applyClip();
                break;
            case RenderCommandType::CustomCallback:
                if (command.customRenderFunc) {
                    command.customRenderFunc({command.x, command.y, command.width, command.height,
                                              command.customRenderUserData});
                    applyClip();
                }
                break;
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
}