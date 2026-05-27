# OpenGL — Technical Reference & Learning Path

---

## File Index

| File | Topic | Level |
|------|-------|-------|
| [opengl_01_fundamentals.md](opengl_01_fundamentals.md) | Pipeline, Context & State Machine | Fundamental |
| [opengl_02_shaders_glsl.md](opengl_02_shaders_glsl.md) | Shaders & GLSL | Fundamental |
| [opengl_03_buffers_textures.md](opengl_03_buffers_textures.md) | VAO / VBO / EBO & Textures | Fundamental |
| [opengl_04_transformations_camera.md](opengl_04_transformations_camera.md) | Transformations & Camera | Core |
| [opengl_05_lighting.md](opengl_05_lighting.md) | Lighting & Materials | Core |
| [opengl_06_advanced_rendering.md](opengl_06_advanced_rendering.md) | Framebuffers, Blending & Stencil | Intermediate |
| [opengl_07_shadows_effects.md](opengl_07_shadows_effects.md) | Shadows, Normal Maps & HDR | Advanced |
| [opengl_08_modern_opengl.md](opengl_08_modern_opengl.md) | Compute Shaders, DSA & Modern GL | Advanced |
| [opengl_09_performance.md](opengl_09_performance.md) | Performance & Profiling | Advanced |
| [opengl_10_interview_topics.md](opengl_10_interview_topics.md) | Interview Hot Topics | Interview |

---

## OpenGL Version Timeline

| Version | Year | Key Feature |
|---------|------|-------------|
| 1.0 | 1992 | Fixed-function pipeline |
| 2.0 | 2004 | GLSL shaders |
| 3.3 | 2010 | Core profile (remove deprecated) |
| 4.0 | 2010 | Tessellation shaders |
| 4.3 | 2012 | Compute shaders, SSBOs |
| 4.5 | 2014 | Direct State Access (DSA) |
| 4.6 | 2017 | SPIR-V support |

> Modern OpenGL = **3.3 Core Profile** as the baseline. Avoid legacy "immediate mode" (`glBegin`/`glEnd`).

---

## The Rendering Pipeline (Brief)

```
CPU                          GPU
─────────────────────────────────────────────────────────
[Vertices in RAM]
     │ glDrawArrays / glDrawElements
     ▼
[Vertex Shader]    ← programmable — transform positions
     ▼
[Primitive Assembly]
     ▼
[Geometry Shader]  ← optional programmable
     ▼
[Rasterization]    ← fixed — converts triangles to fragments
     ▼
[Fragment Shader]  ← programmable — compute final color
     ▼
[Per-Fragment Tests] ← depth, stencil (fixed)
     ▼
[Framebuffer]
```

---

## 4-Week Study Plan

**Week 1 — Foundation**
- File 01: Pipeline, context, state machine
- File 02: Shaders and GLSL
- File 03: Buffers and textures

**Week 2 — 3D**
- File 04: Transformations and camera
- File 05: Lighting and materials

**Week 3 — Advanced Rendering**
- File 06: Framebuffers, blending, stencil
- File 07: Shadows, normal maps, HDR

**Week 4 — Professional & Interview**
- File 08: Modern OpenGL (compute, DSA)
- File 09: Performance profiling and optimization
- File 10: Interview hot topics

---

## Key Libraries

| Library | Purpose |
|---------|---------|
| **GLFW** | Window + context creation, input |
| **GLAD** | OpenGL function loader (extension loading) |
| **GLM** | Mathematics (vectors, matrices) — header-only |
| **stb_image** | Image loading (PNG, JPG) |
| **Assimp** | 3D model loading (OBJ, FBX, GLTF) |
| **Dear ImGui** | Debug UI overlay |
