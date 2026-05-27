# OpenGL 08 — Modern OpenGL: Compute Shaders, DSA & More

---

## 8.1 Direct State Access (DSA) — OpenGL 4.5+

Traditional OpenGL requires binding objects before operating on them. DSA allows direct operations without binding:

```cpp
// OLD (bind-to-edit)
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

// NEW (DSA) — directly specify the object ID
GLuint VBO;
glCreateBuffers(1, &VBO);                                     // note: glCreateBuffers, not glGenBuffers
glNamedBufferData(VBO, sizeof(data), data, GL_STATIC_DRAW);  // no bind needed

// DSA Texture
GLuint tex;
glCreateTextures(GL_TEXTURE_2D, 1, &tex);
glTextureStorage2D(tex, levels, GL_RGBA8, width, height);
glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glGenerateTextureMipmap(tex);

// DSA VAO
GLuint VAO;
glCreateVertexArrays(1, &VAO);
glVertexArrayVertexBuffer(VAO, 0, VBO, 0, stride);   // binding index 0
glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);  // attrib 0 = 3 floats at offset 0
glVertexArrayAttribBinding(VAO, 0, 0);               // attrib 0 uses binding index 0
glEnableVertexArrayAttrib(VAO, 0);

// Bind texture to unit without binding to target first
glBindTextureUnit(0, tex);   // unit 0 ← tex (DSA equivalent of glActiveTexture + glBindTexture)
```

---

## 8.2 Compute Shaders (OpenGL 4.3+)

General-purpose GPU computation — not tied to rendering pipeline:

```glsl
// compute.glsl
#version 430 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
// Work group = 16x16 threads

layout(rgba8, binding = 0) uniform image2D outputImage;   // write target

uniform sampler2D inputTex;  // read source

void main() {
    ivec2 id    = ivec2(gl_GlobalInvocationID.xy);  // unique thread ID
    ivec2 size  = imageSize(outputImage);

    if (id.x >= size.x || id.y >= size.y) return;  // boundary guard

    vec2 uv   = (vec2(id) + 0.5) / vec2(size);
    vec4 col  = texture(inputTex, uv);

    // Example: invert colors
    vec4 result = vec4(1.0 - col.rgb, col.a);

    imageStore(outputImage, id, result);  // write to image
}
```

```cpp
// Dispatch compute shader
glUseProgram(computeProgram);
glBindImageTexture(0, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

// Dispatch: one work group per 16×16 block of pixels
glDispatchCompute(
    (width  + 15) / 16,
    (height + 15) / 16,
    1
);

// Memory barrier: ensure compute writes are visible to subsequent reads
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
// Then render using outputTex as a normal texture
```

---

## 8.3 Shader Storage Buffer Objects (SSBO)

Large buffers accessible from any shader stage:

```glsl
// In shader
layout(std430, binding = 0) buffer ParticleBuffer {
    vec4 positions[];  // variable-length array
    vec4 velocities[];
} particles;

void main() {
    uint id = gl_GlobalInvocationID.x;
    particles.positions[id].xyz  += particles.velocities[id].xyz * 0.016;
    // gravity
    particles.velocities[id].y   -= 9.8 * 0.016;
}
```

```cpp
// C++ side
struct Particle { glm::vec4 pos, vel; };

GLuint SSBO;
glGenBuffers(1, &SSBO);
glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SSBO);
glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(Particle), data, GL_DYNAMIC_DRAW);
```

SSBO vs UBO:
| | UBO | SSBO |
|-|-----|------|
| Max size | ~16KB (usually 64KB) | GBs (GL_MAX_SHADER_STORAGE_BLOCK_SIZE) |
| Access | Read-only in shader | Read+Write |
| Layout | `std140` | `std430` (tighter) |
| Atomic ops | No | Yes (`atomicAdd`, etc.) |

---

## 8.4 Persistent Mapped Buffers

Map a buffer permanently — CPU writes directly to GPU-visible memory each frame without `glBufferSubData` overhead:

```cpp
GLuint VBO;
glGenBuffers(1, &VBO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);
GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
glBufferStorage(GL_ARRAY_BUFFER, size, nullptr, flags);  // immutable storage

// Map once — keep pointer forever
void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size, flags);

// Each frame: write directly
memcpy(ptr, newVertexData, dataSize);
// GL_MAP_COHERENT_BIT: no need for glFlushMappedBufferRange — changes visible immediately
// Use triple-buffering to avoid writing while GPU reads
```

---

## 8.5 Instanced Rendering

Draw many copies of the same mesh in one draw call:

```cpp
// Per-instance data (e.g., model matrix for each instance)
glm::mat4 modelMatrices[1000];
// ... fill modelMatrices ...

GLuint instanceVBO;
glGenBuffers(1, &instanceVBO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(glm::mat4), modelMatrices, GL_STATIC_DRAW);

// mat4 needs 4 attrib locations (each vec4 = one attrib)
for (int i = 0; i < 4; i++) {
    glEnableVertexAttribArray(3 + i);
    glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE,
                          sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
    glVertexAttribDivisor(3 + i, 1);  // advance once per INSTANCE, not per vertex
}

// Draw 1000 instances in one call
glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, 1000);
```

```glsl
// Vertex shader
layout(location = 0) in vec3 aPos;
layout(location = 3) in mat4 instanceModel;  // per-instance matrix (4 locations: 3,4,5,6)

void main() {
    gl_Position = projection * view * instanceModel * vec4(aPos, 1.0);
}
```

---

## 8.6 Indirect Draw Commands

Let the GPU determine draw parameters — no CPU readback:

```cpp
struct DrawArraysIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};

struct DrawElementsIndirectCommand {
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

GLuint drawCmdBuffer;
glGenBuffers(1, &drawCmdBuffer);
glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawCmdBuffer);
// Filled by CPU or compute shader (GPU culling)

glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                             nullptr, drawCount, 0);
```

**GPU culling pipeline:**
1. Compute shader reads all AABB → tests against frustum.
2. Writes `instanceCount=1` or `instanceCount=0` into the indirect buffer.
3. `glMultiDrawElementsIndirect` reads the buffer — no CPU involved.

---

## 8.7 Bindless Textures (ARB_bindless_texture)

Eliminate texture binding overhead — address textures as 64-bit handles:

```cpp
GLuint64 handle = glGetTextureHandleARB(textureID);
glMakeTextureHandleResidentARB(handle);  // make accessible

// Store handle in SSBO or uniform
struct Material {
    GLuint64 albedoHandle;
    GLuint64 normalHandle;
};
// Pass array of materials to shader via SSBO
```

```glsl
// Shader — sample from handle
#extension GL_ARB_bindless_texture : require
layout(std430, binding = 0) buffer Materials {
    uvec2 albedoHandles[];  // GLuint64 as uvec2
};

void main() {
    sampler2D albedoSampler = sampler2D(albedoHandles[materialID]);
    vec3 col = texture(albedoSampler, uv).rgb;
}
```

---

## Interview Questions

### Easy

**Q: What is a compute shader and how does it differ from a vertex/fragment shader?**

> A compute shader is a general-purpose GPU program not tied to any rendering pipeline stage. It runs arbitrary computations — physics simulation, particle updates, image processing, terrain generation. Unlike vertex/fragment shaders (which process one vertex or fragment at a time with fixed inputs/outputs), a compute shader: has no fixed inputs/outputs — reads/writes via SSBOs and image2D bindings; is dispatched with `glDispatchCompute(x,y,z)` specifying a 3D grid of "work groups"; each work group runs a fixed size of threads (`layout(local_size_x=16,...) in`); threads within a work group can share memory (`shared` qualifier) and synchronize (`barrier()`). Used for: GPU particles, cloth simulation, SSAO, bloom blur, culling, procedural generation.

**Q: What is instanced rendering and why is it important for performance?**

> Instanced rendering draws many copies of the same mesh in a single draw call (`glDrawArraysInstanced` / `glDrawElementsInstanced`). Without instancing: 1000 trees = 1000 separate `glDrawElements` calls, each with CPU → GPU overhead (driver state changes, command encoding). With instancing: 1 draw call on the GPU, each instance reads per-instance data (model matrix, color, LOD) via a per-instance vertex attribute buffer (`glVertexAttribDivisor(attr, 1)`) or via `gl_InstanceID` indexing into an SSBO. The GPU processes all instances in parallel. Speedup: orders of magnitude for large counts. Real-world use: foliage, particles, crowds, procedural objects, UI glyphs.

**Q: What does `glMemoryBarrier` do and when is it needed?**

> GPUs execute operations asynchronously and may reorder reads/writes for performance. `glMemoryBarrier(bits)` inserts a synchronization point: all operations of the specified types issued before the barrier complete before any of the same type issued after. Required when: a compute shader writes to a buffer/texture, and a subsequent rendering pass reads from it — the GPU needs to know to flush compute writes before the fragment shader reads. Example: `glDispatchCompute(...)` writes to an SSBO; `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` ensures writes are visible; then render pass reads the SSBO. Without the barrier, the fragment shader might read stale data (the compute writes may be in-flight or in a write buffer).

---

### Mid

**Q: Explain the difference between `glGenBuffers` + `glBindBuffer` + `glBufferData` and the DSA `glCreateBuffers` + `glNamedBufferData`.**

> `glGenBuffers` only allocates a name (integer ID) — no GPU memory is allocated and the object has no state yet. You must `glBindBuffer` to associate it with a target, then `glBufferData` to allocate and upload. DSA `glCreateBuffers` creates a complete buffer object immediately — it's fully initialized and ready to use without binding. `glNamedBufferData` operates on the object directly by ID. Benefits: (1) No need to save/restore current binding state; (2) Safer — can operate on any buffer without accidentally modifying currently bound buffers; (3) Easier to use in multi-threaded setup code; (4) More debugger-friendly (clear which object is being configured). DSA is the preferred modern API. `glGen*` APIs remain for compatibility.

**Q: What is `std430` layout and when do you use it over `std140`?**

> `std430` is a less padded memory layout for SSBOs (not available for UBOs). Key differences from `std140`: arrays of basic types (float, int, vec2) are tightly packed — an array of `float` packs at 4 bytes each, not 16. `vec3` arrays: each element is 12 bytes (not 16). This matches C struct layout more naturally for arrays of scalars. `std140` forces all array elements to be 16-byte aligned (wasteful for float arrays — 4× the memory). Use `std430` for SSBOs when: storing large arrays of per-element data (particles, vertices, per-instance data), doing compact GPU data processing. Keep `std140` for UBOs (compatibility requirement — UBOs don't support `std430` before GL 4.3 extensions).

---

### Hard

**Q: Implement GPU-accelerated particle simulation using a compute shader + SSBO + instanced rendering.**

> ```glsl
> // compute.glsl — particle update
> #version 430
> layout(local_size_x = 64) in;
>
> struct Particle {
>     vec4 pos;   // xyz = position, w = lifetime
>     vec4 vel;   // xyz = velocity, w = unused
> };
>
> layout(std430, binding = 0) buffer Particles { Particle p[]; };
> uniform float dt;
> uniform float gravity;
>
> void main() {
>     uint id = gl_GlobalInvocationID.x;
>     if (id >= p.length()) return;
>
>     p[id].vel.y -= gravity * dt;
>     p[id].pos.xyz += p[id].vel.xyz * dt;
>     p[id].pos.w   -= dt;  // lifetime
>
>     // Respawn when lifetime expires
>     if (p[id].pos.w <= 0.0) {
>         p[id].pos = vec4((fract(sin(float(id)*127.1)*43758.5)) * 2.0 - 1.0,
>                          1.0, 1.0, 5.0);
>         p[id].vel = vec4(0, 2, 0, 0);
>     }
> }
> ```
> ```cpp
> // Each frame:
> // 1. Compute: update particles
> glUseProgram(computeProg);
> glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SSBO);
> glUniform1f(dtLoc, dt);
> glDispatchCompute((count + 63) / 64, 1, 1);
> glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);  // SSBO used as VBO
>
> // 2. Render: draw instanced billboard quads
> // SSBO bound as per-instance VBO (pos → glVertexAttribDivisor(1))
> glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
> // Zero CPU readback — entirely GPU-resident
> ```
