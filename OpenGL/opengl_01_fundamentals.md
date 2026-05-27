# OpenGL 01 — Pipeline, Context & State Machine

---

## 1.1 OpenGL as a State Machine

OpenGL is a **giant state machine**. Every function call either:
- **Sets state** (current shader, current VAO, blending mode, …)
- **Queries state** (`glGet*`)
- **Issues a draw call** that uses the currently bound state

```cpp
// State persists until changed:
glUseProgram(myShader);    // all subsequent draws use myShader
glBindVertexArray(myVAO);  // all subsequent draws use myVAO
glDrawArrays(GL_TRIANGLES, 0, 36);  // draw using current state
```

---

## 1.2 The Rendering Pipeline

```
Per-Vertex Stage (programmable)
  └─ Vertex Shader: gl_Position = MVP * vec4(pos, 1.0)

Primitive Assembly (fixed)
  └─ Connect vertices into triangles/lines/points

Geometry Shader (optional, programmable)
  └─ Can emit/discard/modify primitives

Clipping (fixed)
  └─ Discard geometry outside view frustum

Rasterization (fixed)
  └─ Triangle → fragments (candidate pixels)
  └─ Interpolate vertex attributes across surface

Per-Fragment Stage (programmable)
  └─ Fragment Shader: compute final RGBA color

Per-Sample Operations (fixed)
  ├─ Depth test  (discard hidden fragments)
  ├─ Stencil test
  ├─ Blending    (alpha compositing)
  └─ Write to framebuffer
```

---

## 1.3 Context Creation with GLFW + GLAD

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* win, int w, int h) {
    glViewport(0, 0, w, h);
}

int main() {
    // ── Init GLFW ──────────────────────────────────────────
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // macOS
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ── Load OpenGL via GLAD ───────────────────────────────
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glViewport(0, 0, 800, 600);

    // ── Main Loop ──────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        // Input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Render
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ... draw calls here ...

        // Swap + Poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
```

---

## 1.4 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(OpenGLApp)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
add_subdirectory(external/glfw)
add_subdirectory(external/glad)

add_executable(app
    src/main.cpp
    src/shader.cpp
)

target_link_libraries(app PRIVATE
    OpenGL::GL
    glfw
    glad
)

target_include_directories(app PRIVATE
    external/glm
    external/stb
)
```

---

## 1.5 Coordinate Systems & NDC

OpenGL uses a right-handed coordinate system. After vertex shader, coordinates must be in **Normalized Device Coordinates (NDC)**: x, y, z all in [-1, 1].

```
World Space ──[View Matrix]──► View Space ──[Projection]──► Clip Space ──[Divide by w]──► NDC
```

| Space | Description |
|-------|-------------|
| Object/Model | Local to the mesh |
| World | After Model matrix transform |
| View/Camera | After View matrix — camera at origin |
| Clip | After Projection matrix |
| NDC | After perspective divide (`xyz/w`) — [-1,1] |
| Screen | After viewport transform → pixel coordinates |

---

## 1.6 Error Checking

```cpp
// Synchronous error check (slow — for debugging only)
GLenum err;
while ((err = glGetError()) != GL_NO_ERROR) {
    std::cerr << "GL Error: 0x" << std::hex << err << "\n";
}

// Macro wrapper
#define GL_CHECK(x) \
    x; \
    { GLenum e = glGetError(); \
      if (e != GL_NO_ERROR) \
          fprintf(stderr, "GL error %x at %s:%d\n", e, __FILE__, __LINE__); }

// Modern: Debug Callback (OpenGL 4.3+) — zero-overhead in release
void APIENTRY gl_debug_callback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    fprintf(stderr, "[GL DEBUG] %s\n", message);
}

// Enable during init:
glEnable(GL_DEBUG_OUTPUT);
glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
glDebugMessageCallback(gl_debug_callback, nullptr);
glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION,
                      0, nullptr, GL_FALSE);
```

---

## 1.7 OpenGL Object Naming Pattern

Every OpenGL resource follows the same pattern:

```cpp
GLuint id;
glGenXxx(1, &id);       // allocate name
glBindXxx(target, id);  // bind to a target slot (makes it "current")
glXxxParameter(...);    // configure current object
// ... use ...
glDeleteXxx(1, &id);    // free
```

Example targets:
| Object | Gen | Bind Target |
|--------|-----|-------------|
| Buffer | `glGenBuffers` | `GL_ARRAY_BUFFER`, `GL_ELEMENT_ARRAY_BUFFER` |
| Texture | `glGenTextures` | `GL_TEXTURE_2D`, `GL_TEXTURE_CUBE_MAP` |
| VAO | `glGenVertexArrays` | `GL_VERTEX_ARRAY` |
| Framebuffer | `glGenFramebuffers` | `GL_FRAMEBUFFER` |

---

## 1.8 Minimal Working Example — Colored Triangle

```cpp
// Vertex data: 3 vertices, each with position (x,y,z) and color (r,g,b)
float vertices[] = {
    // position          // color
     0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // top — red
    -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // left — green
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // right — blue
};

// Setup (once, before main loop)
GLuint VAO, VBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);

glBindVertexArray(VAO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

// Position attribute (location = 0)
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
// Color attribute (location = 1)
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);

// Draw (in main loop)
glUseProgram(shaderProgram);
glBindVertexArray(VAO);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

---

## Interview Questions

### Easy

**Q: What is the difference between OpenGL Core Profile and Compatibility Profile?**

> **Core Profile** removes all deprecated OpenGL 1.x/2.x features (immediate mode `glBegin`/`glEnd`, fixed-function lighting, `glMatrixMode`, etc.). Code must use VAOs, VBOs, and shaders. This is the modern, portable profile — required on macOS and preferred everywhere. **Compatibility Profile** retains all legacy features alongside modern ones. It allows using `glBegin`/`glEnd` and the fixed-function pipeline. Used when maintaining old codebases. New code should always target Core Profile 3.3+.

**Q: What does `glfwSwapBuffers` do and why is double buffering used?**

> `glfwSwapBuffers` exchanges the back buffer and front buffer. **Double buffering**: one buffer is displayed (front), while the next frame is being rendered into the other (back). When rendering is complete, they swap. Without double buffering, the user would see partially-rendered frames (tearing) as the GPU writes to the same buffer being displayed. The swap happens in sync with the monitor's vertical blank (`glfwSwapInterval(1)` = vsync), preventing screen tearing. On monitors above 60Hz, triple buffering or G-Sync/FreeSync allows higher effective frame rates.

**Q: What does `glViewport` do?**

> `glViewport(x, y, width, height)` defines the rectangular region of the window where OpenGL renders. After the NDC-to-screen transform, NDC coordinates are mapped to pixel coordinates within this rectangle. Called when the window is resized (via a framebuffer size callback) to keep the rendering filling the full window. You can also use multiple viewports in one window (split-screen) by calling `glViewport` before each draw call with different regions. Forgetting to call `glViewport` after a resize causes the rendered image to appear in only part of the window.

---

### Mid

**Q: Explain the pipeline stages between a vertex and its final pixel color.**

> 1. **Vertex shader**: transforms vertex position from object space to clip space (`gl_Position = MVP * vec4(pos, 1.0)`). Passes data (normals, UVs, colors) to the next stage. 2. **Primitive assembly**: groups processed vertices into triangles. 3. **Rasterization**: determines which fragments (potential pixels) each triangle covers. Linearly interpolates vertex attributes (UVs, normals, colors) across the triangle's surface using barycentric coordinates. 4. **Fragment shader**: receives the interpolated attributes, samples textures, computes the final RGBA color (`FragColor`). 5. **Depth test**: discards the fragment if a closer fragment was already written to this pixel. 6. **Blending** (if enabled): combines the fragment's color with what's already in the framebuffer using the alpha channel. 7. **Framebuffer write**: writes the final color to the back buffer.

**Q: What is a VAO and why is it needed?**

> A VAO (Vertex Array Object) stores the **configuration** of how vertex data is laid out in VBOs: which buffers are bound, how attributes map to buffer data (via `glVertexAttribPointer`), and which attributes are enabled. Without a VAO, you'd have to re-call `glVertexAttribPointer` and `glEnableVertexAttribArray` before every draw call. With a VAO: configure once at setup, then bind the VAO before drawing to restore all the configuration in one call. The VAO does not store vertex data — the data lives in the VBO. The VAO just records the instructions for interpreting that data.

---

### Hard

**Q: What is the difference between `GL_STATIC_DRAW`, `GL_DYNAMIC_DRAW`, and `GL_STREAM_DRAW` for `glBufferData`?**

> These hints tell the driver how to optimize memory placement — they are **hints, not guarantees**. `GL_STATIC_DRAW`: data uploaded once, used many times for drawing. Driver places the buffer in fast GPU VRAM. `GL_DYNAMIC_DRAW`: data modified repeatedly, used many times. Driver may place it in memory accessible by both CPU and GPU (AGP/PCIe aperture or host-visible VRAM). `GL_STREAM_DRAW`: data modified every frame, used a few times. Driver may use a staging area. For CPU-updated data (particles, UI), use `GL_DYNAMIC_DRAW` or map the buffer with `GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT` and write directly to avoid an extra copy. On modern drivers, `glBufferSubData` or persistent-mapped buffers often outperform re-uploading with `glBufferData`.
