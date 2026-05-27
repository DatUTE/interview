# OpenGL 02 — Shaders & GLSL

---

## 2.1 Shader Types

| Shader | Stage | Purpose |
|--------|-------|---------|
| Vertex shader | Per-vertex | Transform positions, pass attributes |
| Geometry shader | Per-primitive | Optional — emit/modify/discard primitives |
| Tessellation | Per-patch | Subdivide geometry on GPU |
| Fragment shader | Per-fragment | Compute final color |
| Compute shader | General | General-purpose GPU computation |

---

## 2.2 Minimal Shader Pair (3.3 Core)

**Vertex Shader** (`vert.glsl`):
```glsl
#version 330 core

layout (location = 0) in vec3 aPos;    // vertex attribute 0
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 MVP;   // Model-View-Projection matrix

void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
```

**Fragment Shader** (`frag.glsl`):
```glsl
#version 330 core

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texture1;
uniform vec4      tintColor;

void main() {
    FragColor = texture(texture1, TexCoord) * tintColor;
}
```

---

## 2.3 Compiling and Linking Shaders (C++)

```cpp
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

// Read file to string
std::string read_file(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Compile a shader, return id (0 on fail)
GLuint compile_shader(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

// Link program
GLuint create_program(const char* vert_path, const char* frag_path) {
    std::string vs = read_file(vert_path);
    std::string fs = read_file(frag_path);

    GLuint vert = compile_shader(GL_VERTEX_SHADER,   vs.c_str());
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fs.c_str());

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vert);  // shaders no longer needed after linking
    glDeleteShader(frag);
    return prog;
}
```

---

## 2.4 GLSL Data Types

```glsl
// Scalars
bool  b = true;
int   i = 42;
uint  u = 42u;
float f = 3.14;

// Vectors (2, 3, or 4 components)
vec2  uv  = vec2(0.5, 0.5);
vec3  pos = vec3(1.0, 2.0, 3.0);
vec4  col = vec4(1.0, 0.0, 0.0, 1.0);  // rgba
ivec3 ipos;
uvec2 uuv;
bvec4 mask;

// Matrices (column-major)
mat2 m2;  mat3 m3;  mat4 m4;

// Swizzling — rearrange components freely
vec4 c = vec4(1.0, 0.5, 0.2, 1.0);
vec3 rgb = c.rgb;     // (1.0, 0.5, 0.2)
vec2 st  = c.zx;     // (0.2, 1.0)   — arbitrary order
float r  = c.r;       // or c.x or c.s

// Samplers
sampler2D   tex;
sampler3D   vol;
samplerCube cube;
sampler2DShadow shadow;  // for shadow maps

// Arrays
float weights[4];
vec3  lights[8];
```

---

## 2.5 Built-in Variables

**Vertex shader:**
```glsl
gl_Position   // vec4 — clip-space output position (mandatory)
gl_PointSize  // float — size of points (GL_POINTS mode)
gl_VertexID   // int — current vertex index
gl_InstanceID // int — instance index (instanced rendering)
```

**Fragment shader:**
```glsl
gl_FragCoord  // vec4 — window-space position (x, y, z, 1/w)
gl_FrontFacing// bool — true if front face
gl_FragDepth  // float — override depth (expensive, disables early-z)
// Output:
out vec4 FragColor;  // writes to color attachment 0
```

---

## 2.6 Uniforms

```cpp
// Setting uniforms from C++
glUseProgram(prog);  // must be active

GLint loc = glGetUniformLocation(prog, "MVP");

// Float matrix (transpose=GL_FALSE because GLM is column-major like OpenGL)
glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));

// Common setters:
glUniform1i(glGetUniformLocation(prog, "texture1"), 0);   // sampler unit
glUniform1f(glGetUniformLocation(prog, "time"),     t);
glUniform3fv(glGetUniformLocation(prog, "lightPos"), 1, glm::value_ptr(lp));
glUniform4f (glGetUniformLocation(prog, "color"), 1,0,0,1);

// Cache locations (slow to call per-frame)
struct Uniforms {
    GLint mvp, lightPos, albedo;
};
Uniforms u;
u.mvp      = glGetUniformLocation(prog, "MVP");
u.lightPos = glGetUniformLocation(prog, "lightPos");
```

---

## 2.7 Uniform Buffer Objects (UBO)

Group uniforms shared across multiple shader programs into a block — avoids redundant uploads:

```glsl
// In shader
layout (std140, binding = 0) uniform Matrices {
    mat4 view;
    mat4 projection;
};
```

```cpp
// In C++
struct MatricesUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

GLuint ubo;
glGenBuffers(1, &ubo);
glBindBuffer(GL_UNIFORM_BUFFER, ubo);
glBufferData(GL_UNIFORM_BUFFER, sizeof(MatricesUBO), nullptr, GL_DYNAMIC_DRAW);
glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);  // binding point 0

// Update per-frame
MatricesUBO data { viewMatrix, projMatrix };
glBindBuffer(GL_UNIFORM_BUFFER, ubo);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(data), &data);
```

---

## 2.8 Shader Class (C++ Wrapper)

```cpp
class Shader {
public:
    GLuint id;

    Shader(const char* vertPath, const char* fragPath) {
        id = create_program(vertPath, fragPath);
    }
    ~Shader() { glDeleteProgram(id); }

    void use() const { glUseProgram(id); }

    void setInt  (const std::string& name, int v)               const { glUniform1i (loc(name), v); }
    void setFloat(const std::string& name, float v)             const { glUniform1f (loc(name), v); }
    void setVec3 (const std::string& name, const glm::vec3& v)  const { glUniform3fv(loc(name), 1, glm::value_ptr(v)); }
    void setMat4 (const std::string& name, const glm::mat4& m)  const { glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(m)); }

private:
    GLint loc(const std::string& name) const {
        return glGetUniformLocation(id, name.c_str());
    }
};
```

---

## 2.9 GLSL Built-in Functions

```glsl
// Math
float a = abs(-3.0);          // 3.0
float f = floor(3.7);         // 3.0
float c = ceil(3.2);          // 4.0
float m = mod(7.0, 3.0);      // 1.0
float p = pow(2.0, 10.0);     // 1024
float s = sqrt(4.0);          // 2.0
float e = exp(1.0);           // e
float l = log(2.718);         // ~1.0
float x = clamp(1.5, 0.0, 1.0); // 1.0
float mix_val = mix(0.0, 10.0, 0.3); // 3.0 (linear interpolation)
float sm = smoothstep(0.0, 1.0, 0.5); // smooth curve

// Trigonometry
float s2 = sin(3.14159 / 2.0); // 1.0
float r = radians(180.0);       // π

// Vector
vec3 n   = normalize(vec3(1, 1, 0)); // unit vector
float d  = dot(vec3(1,0,0), vec3(0,1,0)); // 0.0
vec3 cr  = cross(vec3(1,0,0), vec3(0,1,0)); // vec3(0,0,1)
float ln = length(vec3(3,4,0)); // 5.0
float ds = distance(p1, p2);

// Texture
vec4 col = texture(tex, uv);
vec4 col2 = textureLod(tex, uv, 2.0);    // explicit mip level
vec4 col3 = textureGrad(tex, uv, dx, dy); // custom derivatives
vec4 col4 = texelFetch(tex, ivec2(x,y), 0); // exact texel, no filtering
```

---

## Interview Questions

### Easy

**Q: What is the difference between `in`, `out`, and `uniform` in GLSL?**

> `in`: input variable. In the vertex shader, `in` variables receive per-vertex data from the VAO (position, normal, UV). Between stages, `out` in one shader becomes `in` in the next — the values are interpolated across the primitive during rasterization. `out`: output variable. Vertex shader writes `gl_Position` and passes data to the next stage via `out` variables. Fragment shader writes to `out` variables that map to framebuffer attachments. `uniform`: a value set from the CPU side that is the same for every vertex and fragment in a draw call — does not change per vertex. Used for transformation matrices, texture samplers, light positions, time, etc. Uniforms persist across draw calls until explicitly changed.

**Q: What happens if you forget to call `glUseProgram` before setting a uniform?**

> `glGetUniformLocation` finds the location in the specified program (passed as argument). `glUniform*` sets the uniform in the **currently bound program** (the one last set with `glUseProgram`). If no program is bound, the behavior is undefined / the call is ignored. If a different program is bound, you set the uniform in the wrong program — a silent bug that produces wrong rendering. Best practice: always call `glUseProgram(prog)` before any `glUniform*` calls. Alternatively, use the DSA API (`glProgramUniform*`) which takes the program ID directly and doesn't require binding.

**Q: What is swizzling in GLSL and give two examples of its use?**

> Swizzling lets you access and rearrange vector components using `.xyzw` (position), `.rgba` (color), or `.stpq` (texture) suffixes in any order, any count. Example 1 — extracting a subvector: `vec4 color = vec4(1,0.5,0.2,1); vec3 rgb = color.rgb;` — takes the first three components. Example 2 — reversing or repeating: `vec2 uv = someVec.yx;` — swaps x and y; `vec3 gray = vec3(luminance.rrr);` — broadcasts a single value to all three channels. Swizzling is a compile-time construct — it compiles to zero-cost register operations on the GPU (no actual data movement).

---

### Mid

**Q: What is `std140` layout in UBOs and why is it important?**

> `std140` is a standardized memory layout for uniform blocks that is identical between the CPU (C++ struct) and GPU (GLSL block). Without it, the GPU driver may pad/align the struct differently from the CPU struct, causing data mismatch. `std140` rules: scalars are aligned to their size; `vec2` aligned to 8 bytes; `vec3` and `vec4` aligned to 16 bytes; arrays of scalars/vectors have each element aligned to 16 bytes; matrices treated as arrays of column vectors. Implication: a `mat4` (4×4 float) is 64 bytes, correctly aligned. But a `bool` or `float` takes 16 bytes in an array (not 4!) — so pack arrays of small types into `vec4` manually. Alternative: `std430` (for SSBOs) has tighter packing.

**Q: Explain the difference between `texture()` and `texelFetch()` in GLSL.**

> `texture(sampler, uv)` samples a texture using **normalized** UV coordinates ([0,1]), applies filtering (bilinear, trilinear with mipmaps) and wrapping (repeat, clamp). The GPU's texture unit does the sampling with hardware interpolation. `texelFetch(sampler, ivec2(x,y), lod)` accesses an **exact texel** at integer pixel coordinates with no filtering — returns the raw texel value. Use `texture` for general rendering (sampling textures on surfaces). Use `texelFetch` for: image processing where you need exact pixel access, reading data textures (not color textures), G-buffer access in deferred rendering, or any case where filtering would corrupt your data.

---

### Hard

**Q: How do you implement a Phong lighting calculation entirely in GLSL?**

```glsl
// Fragment Shader — Phong lighting
#version 330 core
in vec3 FragPos;    // world-space position
in vec3 Normal;     // world-space normal (from vertex shader)
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform sampler2D diffuseTex;

void main() {
    vec3 albedo = texture(diffuseTex, TexCoord).rgb;
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);   // light dir
    vec3 V = normalize(viewPos  - FragPos);   // view dir
    vec3 R = reflect(-L, N);                  // reflection dir

    // Ambient
    vec3 ambient = 0.1 * albedo;

    // Diffuse (Lambertian)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightColor * albedo;

    // Specular (Phong)
    float shininess = 32.0;
    float spec = pow(max(dot(V, R), 0.0), shininess);
    vec3 specular = spec * lightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

> Phong model: `color = ambient + diffuse + specular`. Ambient prevents fully black shadows. Diffuse uses `max(dot(N,L), 0)` — Lambert's cosine law. Specular uses `pow(max(dot(V,R), 0), shininess)` where R is the light reflection. **Blinn-Phong** improvement: replace `dot(V,R)` with `dot(N,H)` where `H = normalize(L+V)` (halfway vector) — avoids artifacts at grazing angles and is cheaper to compute.
