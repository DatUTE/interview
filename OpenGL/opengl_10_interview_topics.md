# OpenGL 10 — Interview Hot Topics

---

## Q1: Explain the OpenGL Rendering Pipeline End-to-End

```
CPU side:
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0)
         │
         ▼
  [Vertex Fetch]
  VAO → read indices from EBO, fetch vertex data from VBO
         │
         ▼
  [Vertex Shader] ← programmable
  gl_Position = projection * view * model * vec4(pos, 1.0);
  Pass: Normal, TexCoord, FragPos to next stage
         │
         ▼
  [Primitive Assembly]
  Group 3 vertices into triangles
         │
         ▼
  [Clipping]
  Discard / clip triangles outside [-1,1] cube
         │
         ▼
  [Rasterization]
  Triangle → screen-space fragments
  Interpolate: TexCoord, Normal, FragPos using barycentric weights
         │
         ▼
  [Fragment Shader] ← programmable
  Sample textures, compute lighting → FragColor
         │
         ▼
  [Per-Fragment Tests]
  Scissor test → stencil test → depth test → blend → framebuffer write
```

---

## Q2: State Machine — What "State" Is Active During a Draw Call

At the moment of a draw call, the following state determines the output:

```
Shader program    glUseProgram(prog)
Vertex input      glBindVertexArray(VAO)
Uniform values    glUniform*(loc, val)
Texture bindings  glActiveTexture + glBindTexture (per unit)
Framebuffer       glBindFramebuffer(FBO)
Viewport          glViewport(x, y, w, h)
Depth test        glEnable(GL_DEPTH_TEST), glDepthFunc
Stencil test      glEnable(GL_STENCIL_TEST), glStencilFunc/Op
Blending          glEnable(GL_BLEND), glBlendFunc
Culling           glEnable(GL_CULL_FACE), glCullFace, glFrontFace
```

---

## Q3: The MVP Matrix — Where Each Transform Happens

```glsl
// In vertex shader, right-to-left application:
gl_Position = Projection * View * Model * vec4(localPos, 1.0);
//              ↑            ↑      ↑
//              │            │      └── 1. Local → World
//              │            └───────── 2. World → Camera space
//              └────────────────────── 3. Camera → Clip space
//
// After vertex shader: perspective divide (xyz/w) → NDC
// After rasterization: viewport transform → pixels
```

Normal vectors need the **normal matrix**: `transpose(inverse(mat3(model)))` — protects normals from non-uniform scale distortion.

---

## Q4: Z-Fighting — Cause and Fix

```
Cause:
  Two surfaces at nearly identical depth.
  Floating-point precision insufficient to distinguish them.
  Result: alternating pixels from each surface → flickering.

Fixes (best to worst):
  1. Don't place coplanar surfaces — add physical separation
  2. Increase near plane (0.1 not 0.001) — more depth precision near camera
  3. Reduce far/near ratio (< 10,000x)
  4. Polygon offset: glPolygonOffset(-1, -1) — shift one surface slightly
  5. Reverse-Z: depth 1=near, 0=far with float32 — much better distribution
  6. 32-bit depth buffer (vs 24-bit default)
```

---

## Q5: Blending and Transparency — Order Problem

```
Problem:
  Blending: dst = src.rgb * src.a + dst.rgb * (1 - src.a)
  Requires dst to contain the correct background first.
  If transparent object A is drawn before B behind it:
  dst = A blended onto black → then B blended on top of wrong A.

Solution:
  1. Draw all opaque objects first (depth writes ON)
  2. Sort transparent objects back-to-front by distance to camera
  3. Draw transparent objects back-to-front (depth writes OFF)

Limitation: still fails for intersecting transparents.
Advanced: OIT (Order-Independent Transparency):
  - Depth peeling: N passes, peel front-most transparent each pass
  - Weighted Blended OIT: approximation, single pass, GPU-friendly
  - Fragment lists (linked list in SSBO): exact but memory-heavy
```

---

## Q6: Texture Filtering — Nearest vs Linear vs Mipmapping

```
GL_NEAREST:
  Take closest texel. Fast, pixelated look. Good for pixel art.

GL_LINEAR:
  Bilinear interpolation of 4 nearest texels. Smooth but blurry up close.

GL_LINEAR_MIPMAP_NEAREST:
  Trilinear? No — choose nearest mip level, linear within.

GL_LINEAR_MIPMAP_LINEAR:
  Trilinear: linear between 4 texels AND between 2 mip levels.
  Best quality for minification. GPU hardware accelerated.

Anisotropic filtering (AF):
  Samples along the surface footprint direction (not square).
  Eliminates blurriness on surfaces viewed at oblique angles.
  glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f)
```

---

## Q7: Depth Buffer — Format and Precision

```
Standard: GL_DEPTH_COMPONENT24 (24-bit fixed-point)
Better:   GL_DEPTH_COMPONENT32F (32-bit float) — much more precision

Distribution of depth precision (perspective projection):
  ┌──────────────────────────────────────────────────────┐
  │ ████████████████████████████░░░░░░░░░░░░░░░░░░░░░░░ │
  │ ← near                                         far → │
  └──────────────────────────────────────────────────────┘
  Most precision near the camera, almost none at the far plane.
  Example (near=0.1, far=1000): 50% of precision in first 1 meter.

Reverse-Z (depth=1 near, depth=0 far):
  Avoids precision loss — float32 has most precision near 0.
  glDepthRangef(1.0, 0.0)  and  GL_GEQUAL depth test
  Result: uniform precision across whole depth range.
```

---

## Q8: Shader Compilation — Precompile and Cache

```cpp
// Compile at startup (not during gameplay):
GLuint vert = compile_shader(GL_VERTEX_SHADER, vsSource);
GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fsSource);
GLuint prog = glCreateProgram();
glAttachShader(prog, vert); glAttachShader(prog, frag);
glLinkProgram(prog);

// Binary cache (GL_ARB_get_program_binary):
GLint formats; glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
if (formats > 0) {
    GLint len; glGetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &len);
    std::vector<uint8_t> buf(len); GLenum fmt;
    glGetProgramBinary(prog, len, nullptr, &fmt, buf.data());
    // Save buf to disk

    // Next launch — reload from disk (fast):
    GLuint prog2 = glCreateProgram();
    glProgramBinary(prog2, fmt, buf.data(), buf.size());
}

// SPIR-V shaders (GL 4.6 / ARB_gl_spirv):
// Compile offline with glslangValidator → .spv file
// Load at runtime → no glsl compilation cost
glShaderBinary(1, &vert, GL_SHADER_BINARY_FORMAT_SPIR_V, spv_data, spv_size);
glSpecializeShader(vert, "main", 0, nullptr, nullptr);
```

---

## Q9: Vertex Formats — Optimization

```cpp
// WASTEFUL: 3 floats per component × 4 bytes = 48 bytes per vertex
struct VertexWaste {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    // 32 bytes
};

// OPTIMIZED: pack normals as GL_INT_2_10_10_10_REV (4 bytes total)
// pack UVs as GL_HALF_FLOAT (4 bytes)
// positions as GL_FLOAT (12 bytes) → total 20 bytes per vertex
struct VertexPacked {
    float    px, py, pz;    // 12 bytes — keep float for precision
    uint32_t normal;        // 4 bytes — 10/10/10/2 packed
    uint16_t u, v;          // 4 bytes — half-float
    // 20 bytes total — 37.5% less bandwidth
};

// Tell OpenGL how to unpack:
glVertexAttribPointer(1, 4, GL_INT_2_10_10_10_REV, GL_TRUE, stride, normalOffset);
glVertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, stride, uvOffset);
```

---

## Q10: OpenGL vs Vulkan vs Metal — When to Choose

| Aspect | OpenGL 4.x | Vulkan | Metal |
|--------|-----------|--------|-------|
| Platform | All | All except Apple | Apple only |
| API complexity | Low | Very high | Medium |
| Driver overhead | High (driver does a lot) | Very low (explicit) | Low |
| Multi-thread | Poor | Excellent | Good |
| Render passes | Implicit | Explicit | Explicit |
| Memory | Driver manages | Explicit allocation | Explicit |
| Debugging | Good (debug callback) | RenderDoc + validation | Xcode GPU debugger |
| When to use | Learning, prototypes, tools | AAA games, mobile, VR | Apple ecosystem |

---

## Additional Practice Questions

### Easy

**Q: What is face culling and which face is culled by default?**

> Face culling discards triangles that face away from the camera — their interior is never visible for solid objects, so rendering them wastes GPU work. `glEnable(GL_CULL_FACE)` enables culling. `glCullFace(GL_BACK)` (default) culls back faces. Front vs back determined by winding order: counter-clockwise winding (viewed from front) = front face (OpenGL default, set by `glFrontFace(GL_CCW)`). Culling saves ~50% of vertex/fragment work for closed meshes. Disable culling for: transparent objects (need both faces), flat planes viewed from both sides, debugging (seeing inside meshes). Wrong winding order → entire model appears invisible (culling discards all faces).

**Q: What is `gl_FragCoord.z` and how is it related to the depth buffer?**

> `gl_FragCoord.z` is the fragment's depth value in the range [0, 1], where 0 = near plane and 1 = far plane (with standard depth range). This is the value that the GPU writes to the depth buffer if the fragment passes the depth test. With perspective projection, it is non-linearly distributed — most of the [0,1] range is concentrated near the near plane. To get a linear depth for visualization: `float linear = (2.0 * near) / (far + near - gl_FragCoord.z * (far - near))`. Writing to `gl_FragDepth` manually (instead of the default) disables early-z optimization, so avoid it unless necessary.

---

### Mid

**Q: What is the difference between `glTexImage2D` and `glTextureStorage2D`? Why prefer the latter?**

> `glTexImage2D` uploads one mip level and may reallocate GPU memory. The texture's internal format can be changed by subsequent calls. `glTextureStorage2D(tex, levels, GL_RGBA8, w, h)` allocates all mip levels at once with an **immutable** format — no reallocation, no format change allowed. Benefits: drivers can choose optimal memory layout upfront (e.g., compressed/tiled), the GPU can alias-check more safely, and subsequent uploads use `glTexSubImage2D` (no allocation). Always prefer `glTextureStorage2D` (or `glTexStorage2D`) for final textures. Use `glTexImage2D` only when texture size or format needs to change dynamically (rare).

**Q: What is a geometry shader and what are its main use cases?**

> A geometry shader sits between vertex and fragment stages. It receives a complete primitive (triangle, line, or point) and can output zero or more new primitives. Use cases: (1) **Rendering to cubemap faces** in one pass — emit 6 triangles, one per face, using layer output. (2) **Particle billboards** — receive a point, emit a facing quad (4 vertices). (3) **Wireframe overlay** — compute edge distances in geometry shader, use in fragment for smooth wireframe. (4) **Shadow volume extrusion** — silhouette edges. Limitations: geometry shaders are slow on modern GPUs — the variable output size breaks GPU parallelism. Prefer compute shaders for particle generation. Avoid for anything performance-critical.

---

### Hard

**Q: Describe a complete deferred rendering pipeline with PBR, SSAO, and HDR bloom.**

> Pipeline: **G-Buffer pass**: render opaque geometry into 4 textures: `RT0` = albedo (RGB) + metallic (A) as `GL_RGBA8`; `RT1` = normal (RG16F) compressed; `RT2` = roughness(R) + AO(G) + emissive(B); `RT3` = none (depth buffer is the 4th G-buffer). **SSAO pass**: read position (reconstructed from depth+projection) + normal. Output: 1-channel AO texture. Blur (bilateral) to reduce noise. **Lighting pass** (fullscreen quad): for each light, read G-buffer. Evaluate Cook-Torrance BRDF. Accumulate into HDR `GL_R11F_G11F_B10F` buffer. **IBL pass**: add image-based ambient term (irradiance + pre-filter + BRDF LUT). Multiply by SSAO. **Bloom**: extract bright pixels (>1.0 in HDR), downsample 4×, Gaussian blur in ping-pong FBOs, upsample and add. **Tone mapping + gamma**: ACES → gamma 2.2. **TAA** (optional): temporal anti-aliasing using motion vectors + history buffer. Output to screen. Total: ~7–10 render passes, fully GPU-pipelined.

**Q: Explain how you would implement order-independent transparency (OIT) using linked lists in OpenGL.**

> **Setup**: SSBO containing a linked list of fragments per pixel. Atomic counter for next free slot. Head pointer texture (one uint per pixel, init to sentinel). **Fragment shader (OIT geometry pass)**: for each transparent fragment, allocate a slot (`atomicCounterIncrement`), write `{color, depth, next_ptr}` into the fragment list, atomically update the head pointer for this pixel. **Resolve pass** (fullscreen): for each pixel, collect all fragments in the linked list, sort by depth (insertion sort — typically < 8 fragments), composite front-to-back: `color = src.rgb * src.a + color * (1 - src.a)`. Write final color. Performance: GPU divergence is high (different pixels have different list lengths). Memory: worst case O(width × height × max_fragments). Practical limit ~8 layers. Alternative: **Weighted Blended OIT** (McGuire & Bavoil 2013) — single pass, approximate, no sort, O(1) memory — used in many production engines.
