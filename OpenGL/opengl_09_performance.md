# OpenGL 09 — Performance & Profiling

---

## 9.1 GPU Profiling Tools

```cpp
// ── GPU Timer Query ─────────────────────────────────────
GLuint query;
glGenQueries(1, &query);

glBeginQuery(GL_TIME_ELAPSED, query);
// ... draw calls to measure ...
glEndQuery(GL_TIME_ELAPSED);

// Read result (blocks until GPU completes — use previous frame's result)
GLuint64 elapsed_ns;
glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed_ns);
float elapsed_ms = elapsed_ns / 1e6f;

// ── Non-blocking (check if result available) ─────────────
GLint available;
glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
if (available) {
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed_ns);
}
```

**External tools:**
| Tool | Platform | Features |
|------|----------|----------|
| RenderDoc | All | Frame capture, pipeline debugger, shader debugger |
| NVIDIA Nsight | NVIDIA | Deep GPU profiling, warp stats |
| AMD Radeon GPU Profiler | AMD | GPU timing, occupancy |
| Intel GPA | Intel | Frame analysis |
| apitrace | Linux | API call trace and replay |

---

## 9.2 Draw Call Reduction

Draw calls have CPU overhead (~1–5µs each). Reduce them:

**1. Instancing** — already covered (File 08)

**2. Texture Atlases** — pack many textures into one large texture, use UV offsets:
```glsl
// Instead of switching textures per draw call:
vec2 atlasOffset = vec2(col * tileSize, row * tileSize);
vec2 atlasUV = fract(TexCoord) * tileSize / atlasSize + atlasOffset;
vec3 color = texture(atlas, atlasUV).rgb;
```

**3. Batching by material** — sort draw calls by shader/material to minimize state changes:
```cpp
std::sort(drawList.begin(), drawList.end(),
    [](const DrawCall& a, const DrawCall& b) {
        if (a.shaderID != b.shaderID) return a.shaderID < b.shaderID;
        if (a.textureID != b.textureID) return a.textureID < b.textureID;
        return a.mesh < b.mesh;
    });
```

**4. Geometry merging** — merge static objects that share a material into one VBO:
```cpp
// Merge all static trees into one VBO
std::vector<Vertex> mergedVerts;
std::vector<uint32_t> mergedIdx;
for (const auto& tree : staticTrees) {
    uint32_t base = mergedVerts.size();
    for (auto& v : tree.vertices) {
        Vertex wv = v;
        wv.pos = glm::vec3(tree.model * glm::vec4(v.pos, 1.0f));
        mergedVerts.push_back(wv);
    }
    for (auto idx : tree.indices)
        mergedIdx.push_back(base + idx);
}
// One draw call for all static trees
```

**5. Multi-draw indirect** — let GPU decide what to draw (covered in File 08)

---

## 9.3 State Change Cost

State changes have different costs. Order from most to least expensive:

```
Most expensive:
  glUseProgram           ← shader compilation/switch
  glBindFramebuffer      ← RT switch (potentially flush)
  glBindTexture          ← descriptor update
  glBindBuffer           ← bind update
  Uniform updates        ← per-object uniforms
Least expensive:
  glDrawArrays           ← just submits a draw command
```

**Rule**: sort draw calls to minimize state changes, especially shader and framebuffer switches.

---

## 9.4 Occlusion Culling

```cpp
// Hardware occlusion query — test if bounding box is visible
GLuint occlusionQuery;
glGenQueries(1, &occlusionQuery);

// Render bounding box with color/depth writes disabled
glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
glDepthMask(GL_FALSE);

glBeginQuery(GL_ANY_SAMPLES_PASSED, occlusionQuery);
render_bounding_box(object);
glEndQuery(GL_ANY_SAMPLES_PASSED);

glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
glDepthMask(GL_TRUE);

// Next frame: use last frame's result (avoid GPU stall)
GLint visible;
glGetQueryObjectiv(occlusionQuery, GL_QUERY_RESULT, &visible);
if (visible) render_full_object(object);
```

---

## 9.5 LOD — Level of Detail

```cpp
// Choose mesh detail based on distance
float dist = glm::distance(camera.Position, object.position);

Mesh* mesh;
if      (dist < 10.0f)  mesh = &object.meshLOD0;  // high poly
else if (dist < 50.0f)  mesh = &object.meshLOD1;  // medium
else if (dist < 200.0f) mesh = &object.meshLOD2;  // low
else                    mesh = nullptr;            // cull

if (mesh) render(*mesh, object.model);
```

---

## 9.6 Early-Z / Pre-Z Pass

Render opaque geometry depth-only first. Then full render with depth test `GL_EQUAL` — fragment shader only runs for pixels that survive the depth test:

```cpp
// ── Pre-Z pass (no fragment shader needed) ──────────────
glUseProgram(depthOnlyShader);
glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
glDepthFunc(GL_LESS);
for (auto& obj : opaqueObjects) render_depth(obj);

// ── Full lighting pass ───────────────────────────────────
glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
glDepthFunc(GL_EQUAL);   // only pass pixels at exact depth
glDepthMask(GL_FALSE);   // no more depth writes
glUseProgram(lightingShader);
for (auto& obj : opaqueObjects) render_lit(obj);

glDepthFunc(GL_LESS);
glDepthMask(GL_TRUE);
```

Benefit: heavy fragment shaders only run where actually visible. Expensive for GPU-heavy shaders.

---

## 9.7 Buffer Streaming Strategies

For dynamic data (particles, UI, debug geometry) updated every frame:

```cpp
// Strategy 1: Orphan + re-upload
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW); // orphan
glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, data);

// Strategy 2: Persistent mapped triple buffer
const int FRAMES = 3;
// Allocate FRAMES * size contiguous memory
GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
glBufferStorage(GL_ARRAY_BUFFER, size * FRAMES, nullptr, flags);
uint8_t* basePtr = (uint8_t*)glMapBufferRange(GL_ARRAY_BUFFER, 0, size * FRAMES, flags);

int frame = 0;
// Per frame:
int offset = (frame % FRAMES) * size;
memcpy(basePtr + offset, data, dataSize);
glDrawArrays(GL_TRIANGLES, 0, count);  // reads from current frame's slot
frame++;
// Sync: use fences to ensure GPU is done with slot before reuse
```

---

## 9.8 Common Performance Pitfalls

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| Too many draw calls | CPU-bound, GPU idle | Batch, instance, atlas |
| Texture thrashing | GPU stall on texture fetch | Sort by texture, use arrays |
| Overdraw | GPU fill-rate bound | Depth pre-pass, front-to-back sort |
| Uniform updates per object | CPU overhead | UBO, SSBO for per-instance data |
| `glReadPixels` in hot path | GPU stall | PBO (pixel buffer object), async readback |
| Missing mipmaps | Texture cache thrashing | Always generate mipmaps |
| Large mesh, no LOD | Fill-rate + vertex bound | LOD system |
| Synchronous queries | GPU stall | Always read previous frame's result |

---

## 9.9 Pixel Buffer Objects (PBO) — Async Readback

```cpp
GLuint PBO;
glGenBuffers(1, &PBO);
glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO);
glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 4, nullptr, GL_STREAM_READ);

// Initiate async read (returns immediately)
glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);

// Later (next frame): map PBO to get data
glBindBuffer(GL_PIXEL_PACK_BUFFER, PBO);
uint8_t* pixels = (uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
// use pixels...
glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
```

---

## Interview Questions

### Easy

**Q: What is a draw call and why does minimizing them matter?**

> A draw call is a command sent from CPU to GPU (`glDrawArrays`, `glDrawElements`, etc.). Each draw call has overhead: the CPU driver validates state, encodes the command, and submits it to the GPU command queue. On typical hardware this costs 1–5µs CPU time and some GPU setup. For a game targeting 60fps (16.7ms budget), at 5µs per draw call: 3000 draw calls = 15ms of CPU time — nearly the full budget, before any actual game logic. Solutions: batch static objects into fewer large meshes, use instancing for repeated objects, use multi-draw indirect to batch from GPU. Modern console/Vulkan/DX12 APIs have significantly lower call overhead, but OpenGL's driver overhead remains a bottleneck.

**Q: What is overdraw and how do you reduce it?**

> Overdraw occurs when the same pixel is shaded multiple times — later writes overwrite earlier ones. With a heavy fragment shader, rendering a scene with 3× overdraw means 3× the shader cost. Causes: transparent objects, poor draw order, many overlapping UI elements, volumetric effects. Reduction strategies: (1) **Front-to-back rendering** for opaque objects — depth test eliminates hidden fragments before shading. (2) **Early-Z / pre-Z pass** — fill depth buffer cheaply, then main pass only shades surviving pixels. (3) **Portal/sector culling** — never submit geometry in other rooms. (4) **GPU-side occlusion queries** — skip occluded objects entirely. Visualize overdraw with a counting shader (additive blend, count each fragment write).

**Q: What is the purpose of RenderDoc and what information can you get from it?**

> RenderDoc is a free, open-source frame debugger. You attach it to your application and capture a single frame. It shows: (1) **API call list** — every GL function call in order with timing; (2) **Pipeline state** — current VAO, shader, uniforms, textures, blend mode at any call; (3) **Shader debugger** — step through vertex/fragment shader for any specific draw call and inspect variable values; (4) **Texture viewer** — view any texture at any point; (5) **Mesh viewer** — visualize vertex data before/after vertex shader; (6) **Performance counters** — draw call timing, overdraw. Essential for: debugging rendering artifacts, understanding unexpected output, finding performance bottlenecks.

---

### Mid

**Q: Explain the difference between CPU-bound and GPU-bound rendering and how to diagnose each.**

> **CPU-bound**: the GPU is idle waiting for new commands. Symptoms: GPU utilization < 80%, CPU is the bottleneck. Diagnosis: profile CPU frame time — draw call submission loop, culling, physics. Tools: CPU profiler, GPU utilization monitor. Fixes: batch draw calls, reduce per-object state changes, use indirect rendering, frustum cull on CPU. **GPU-bound**: the GPU is at 100% and the CPU is waiting for the GPU. Symptoms: GPU utilization near 100%, CPU idle waiting for vsync/fence. Subtypes: vertex-bound (too many vertices — LOD, tessellation), fragment-bound (expensive fragment shaders, high overdraw — reduce shader complexity, overdraw), memory-bound (texture cache misses — improve texture locality, compress textures). Diagnosis: disable vsync, check GPU timer queries per pass, use Nsight/RGP to see per-pass timing breakdown.

**Q: What is texture compression and which formats are commonly used in OpenGL?**

> Texture compression stores texture data in a compressed format on the GPU — less VRAM, better cache utilization, but with slight quality loss. Decompression is done in hardware at sampling time (zero extra cost). Common formats: `BC1` (DXT1) — 4bpp, RGB, great for diffuse/color maps. `BC3` (DXT5) — 8bpp, RGBA, good for color+alpha. `BC5` — 8bpp, RG, used for normal maps (X,Y components only). `BC7` — 8bpp, best quality for color/alpha. `ASTC` (Adaptive Scalable Texture Compression) — mobile/modern, flexible block sizes, very high quality, variable rates. In OpenGL: `glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, ...)`. Load from `.dds` or `.ktx` files (already compressed). Alternatively, use runtime compression via `glTexImage2D` + `GL_COMPRESSED_RGB` hint — driver compresses lazily.

---

### Hard

**Q: Design a render loop that achieves 1000+ draw calls per frame without being CPU-bound.**

> Key insight: modern GPU command recording is the bottleneck, not GPU execution. Solution stack: (1) **Indirect rendering**: pre-sort and cull objects with compute shader → write `DrawElementsIndirectCommand` structs per visible object → `glMultiDrawElementsIndirect` submits all in one call. (2) **Persistent-mapped command buffer**: write commands directly to GPU-visible memory, no `glBufferSubData`. (3) **Instancing for repeated meshes**: 100 tree variants × 10 trees each = 10 indirect draws, not 1000. (4) **Bindless textures**: no `glBindTexture` between draws — material index in instance data looks up handle from SSBO. (5) **One mega-buffer** (multi-draw buffer): all meshes in one VBO+EBO, differentiated by `baseVertex` and `firstIndex` in indirect commands. (6) **Sparse updates**: only update per-frame data (camera matrices in UBO), not per-object uniforms. Result: 1000 objects → 1–10 actual GPU commands on the CPU, all submission work done by compute shader.
