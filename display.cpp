//=================================================================================================
// Copyright (C) 2025 GRAPE Contributors
//=================================================================================================

#include "display.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <print>
#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Load OpenGL functions via GLFW
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <libcamera/formats.h>

namespace {
//-------------------------------------------------------------------------------------------------
void glfwErrorCallback(int error, const char* description) {
  std::println(stderr, "GLFW Error {}: {}", error, description);
}

//-------------------------------------------------------------------------------------------------
// NOLINTBEGIN
// The following is vibe-coded. I take no responsibility for anything
void convertToRGB(const picam::ImageFrame& frame, std::vector<std::uint8_t>& rgb_data) {
  const auto width = frame.header.size.width;
  const auto height = frame.header.size.height;
  const auto pitch = frame.header.pitch;
  const auto* src = reinterpret_cast<const std::uint8_t*>(frame.pixels.data());

  rgb_data.resize(width * height * 3);
  const auto format = frame.header.format;

  if (format == libcamera::formats::RGB888) {
    for (std::uint32_t y = 0; y < height; ++y) {
      std::memcpy(&rgb_data[y * width * 3], &src[y * pitch], width * 3);
    }
  } else if (format == libcamera::formats::YUYV) {
    // Convert YUYV (YUV 4:2:2) to RGB
    // YUYV format: Y0 U0 Y1 V0 (4 bytes for 2 pixels)
    for (std::uint32_t y = 0; y < height; ++y) {
      const auto* row = &src[y * pitch];
      auto* rgb_row = &rgb_data[y * width * 3];

      for (std::uint32_t x = 0; x < width; x += 2) {
        const int y0 = row[x * 2];
        const int u = row[x * 2 + 1];
        const int y1 = row[x * 2 + 2];
        const int v = row[x * 2 + 3];

        // YUV to RGB conversion (ITU-R BT.601)
        const int c0 = y0 - 16;
        const int c1 = y1 - 16;
        const int d = u - 128;
        const int e = v - 128;

        // Pixel 0
        rgb_row[x * 3] = std::clamp((298 * c0 + 409 * e + 128) >> 8, 0, 255);
        rgb_row[x * 3 + 1] = std::clamp((298 * c0 - 100 * d - 208 * e + 128) >> 8, 0, 255);
        rgb_row[x * 3 + 2] = std::clamp((298 * c0 + 516 * d + 128) >> 8, 0, 255);

        // Pixel 1
        rgb_row[(x + 1) * 3] = std::clamp((298 * c1 + 409 * e + 128) >> 8, 0, 255);
        rgb_row[(x + 1) * 3 + 1] = std::clamp((298 * c1 - 100 * d - 208 * e + 128) >> 8, 0, 255);
        rgb_row[(x + 1) * 3 + 2] = std::clamp((298 * c1 + 516 * d + 128) >> 8, 0, 255);
      }
    }
  } else {
    // Unsupported format - fill with magenta as error indicator
    std::println(stderr, "Warning: Unsupported pixel format ({}), displaying error pattern",
                 format);
    for (std::size_t i = 0; i < rgb_data.size(); i += 3) {
      rgb_data[i] = 255;      // R
      rgb_data[i + 1] = 0;    // G
      rgb_data[i + 2] = 255;  // B
    }
  }
}
// NOLINTEND

}  // namespace

namespace picam {

//-------------------------------------------------------------------------------------------------
struct Display::Impl {
  GLFWwindow* window{ nullptr };
  GLuint texture_id{ 0 };
  ImageFrame::Header current_frame_header{};
  std::vector<std::uint8_t> rgb_buffer;  // For pixel format conversion

  GLuint shader_program{ 0 };
  GLuint vao{ 0 };
  GLuint vbo{ 0 };

  float image_aspect_ratio{ 1.0F };

  void initWindow();
  void initGL();
  void createShaders();
  void setupQuad();
  void uploadTexture(const std::uint8_t* rgb_data, const ImageSize& size) const;
  void updateQuadForLetterbox() const;
  void render() const;
  void cleanup();
};

//-------------------------------------------------------------------------------------------------
void Display::Impl::initWindow() {
  glfwSetErrorCallback(glfwErrorCallback);

  if (glfwInit() == GLFW_FALSE) {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  // Request OpenGL ES 3.0 (compatible with Raspberry Pi 5)
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  static constexpr auto DEFAULT_WIDTH = 1280;
  static constexpr auto DEFAULT_HEIGHT = 720;
  window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Camera Live View", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::createShaders() {
  const char* vertex_shader_source = R"(
    #version 300 es
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    
    void main() {
      gl_Position = vec4(aPos, 0.0, 1.0);
      TexCoord = aTexCoord;
    }
  )";

  const char* fragment_shader_source = R"(
    #version 300 es
    precision mediump float;
    
    in vec2 TexCoord;
    out vec4 FragColor;
    
    uniform sampler2D textureSampler;
    
    void main() {
      FragColor = texture(textureSampler, TexCoord);
    }
  )";

  // Compile vertex shader
  const auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
  glCompileShader(vertex_shader);

  static constexpr auto LOG_BUFFER_SIZE = 512;
  using LogBuffer = std::array<char, LOG_BUFFER_SIZE>;

  GLint success = 0;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    auto info_log = LogBuffer{};
    glGetShaderInfoLog(vertex_shader, info_log.size(), nullptr, info_log.data());
    glDeleteShader(vertex_shader);
    throw std::runtime_error(std::string("Vertex shader compilation failed: ") + info_log.data());
  }

  // Compile fragment shader
  const auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    auto info_log = LogBuffer{};
    glGetShaderInfoLog(fragment_shader, info_log.size(), nullptr, info_log.data());
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    throw std::runtime_error(std::string("Fragment shader compilation failed: ") + info_log.data());
  }

  shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);

  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (success == 0) {
    auto info_log = LogBuffer{};
    glGetProgramInfoLog(shader_program, info_log.size(), nullptr, info_log.data());
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(shader_program);
    throw std::runtime_error(std::string("Shader program linking failed: ") + info_log.data());
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  std::println(stdout, "OpenGL shaders compiled and linked successfully");
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::setupQuad() {
  // Full-screen quad vertices (position + texture coordinates)
  // clang-format off
  constexpr auto QUAD_VERTICES = std::array<float, 16>{
    // positions   // texCoords
    -1.0F,  1.0F,  0.0F, 0.0F,  // top-left
    -1.0F, -1.0F,  0.0F, 1.0F,  // bottom-left
     1.0F, -1.0F,  1.0F, 1.0F,  // bottom-right
     1.0F,  1.0F,  1.0F, 0.0F   // top-right
  };
  // clang-format on

  // Generate and bind VAO
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // Generate and bind VBO (use GL_DYNAMIC_DRAW for letterbox updates)
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES.data(), GL_DYNAMIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);

  // Texture coordinate attribute
  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::initGL() {
  createShaders();
  setupQuad();

  // Create texture
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  std::println(stdout, "OpenGL initialized successfully");
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::uploadTexture(const std::uint8_t* rgb_data, const ImageSize& size) const {
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(size.width),
               static_cast<GLsizei>(size.height), 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_data);
  glBindTexture(GL_TEXTURE_2D, 0);
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::updateQuadForLetterbox() const {
  // Get current window size
  int window_width = 0;
  int window_height = 0;
  glfwGetFramebufferSize(window, &window_width, &window_height);

  if (window_width == 0 || window_height == 0) {
    return;  // Window minimized
  }

  const float window_aspect = static_cast<float>(window_width) / static_cast<float>(window_height);

  // Calculate letterbox dimensions
  float quad_width = 1.0F;
  float quad_height = 1.0F;

  if (window_aspect > image_aspect_ratio) {
    quad_width = image_aspect_ratio / window_aspect;
  } else {
    quad_height = window_aspect / image_aspect_ratio;
  }

  const std::array<float, 16> quad_vertices = {
    // positions                    // texCoords
    -quad_width, quad_height,  0.0F, 0.0F,  // top-left
    -quad_width, -quad_height, 0.0F, 1.0F,  // bottom-left
    quad_width,  -quad_height, 1.0F, 1.0F,  // bottom-right
    quad_width,  quad_height,  1.0F, 0.0F   // top-right
  };

  // Update VBO with new vertices
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad_vertices), quad_vertices.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::render() const {
  int window_width = 0;
  int window_height = 0;
  glfwGetFramebufferSize(window, &window_width, &window_height);
  glViewport(0, 0, window_width, window_height);

  glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);

  updateQuadForLetterbox();

  glUseProgram(shader_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);

  glfwSwapBuffers(window);
}

//-------------------------------------------------------------------------------------------------
void Display::Impl::cleanup() {
  if (texture_id != 0) {
    glDeleteTextures(1, &texture_id);
    texture_id = 0;
  }
  if (vao != 0) {
    glDeleteVertexArrays(1, &vao);
    vao = 0;
  }
  if (vbo != 0) {
    glDeleteBuffers(1, &vbo);
    vbo = 0;
  }
  if (shader_program != 0) {
    glDeleteProgram(shader_program);
    shader_program = 0;
  }
  if (window != nullptr) {
    glfwDestroyWindow(window);
    window = nullptr;
  }
  glfwTerminate();
}

//-------------------------------------------------------------------------------------------------
Display::Display() : impl_(std::make_unique<Impl>()) {
  impl_->initWindow();
  impl_->initGL();
}

//-------------------------------------------------------------------------------------------------
Display::~Display() {
  impl_->cleanup();
}

//-------------------------------------------------------------------------------------------------
void Display::update(const ImageFrame& frame) {
  convertToRGB(frame, impl_->rgb_buffer);
  impl_->uploadTexture(impl_->rgb_buffer.data(), frame.header.size);

  if (frame.header.size.width > 0 && frame.header.size.height > 0) {
    impl_->image_aspect_ratio =
        static_cast<float>(frame.header.size.width) / static_cast<float>(frame.header.size.height);
  }

  impl_->current_frame_header = frame.header;

  impl_->render();
}

//-------------------------------------------------------------------------------------------------
auto Display::processEvents() -> bool {
  glfwPollEvents();
  return glfwWindowShouldClose(impl_->window) == 0;
}

}  // namespace picam
