# OpenGL 03 — VAO / VBO / EBO & Textures

---

## 3.1 VBO — Vertex Buffer Object

A VBO stores raw vertex data on the GPU:

```cpp
float vertices[] = {
    // pos (x,y,z)    // normal     // uv
     0.5f, 0.5f, 0.0f,  0,0,1,   1.0f, 1.0f,
    -0.5f, 0.5f, 0.0f,  0,0,1,   0.0f, 1.0f,
    -0.5f,-0.5f, 0.0f,  0,0,1,   0.0f, 0.0f,
     0.5f,-0.5f, 0.0f,  0,0,1,   1.0f, 0.0f,
};

GLuint VBO;
glGenBuffers(1, &VBO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER,       // target
             sizeof(vertices),      // size in bytes
             vertices,              // data pointer (nullptr = allocate only)
             GL_STATIC_DRAW);       // usage hint
```

**Updating buffer data:**
```cpp
// Replace entire buffer
glBufferData(GL_ARRAY_BUFFER, newSize, newData, GL_DYNAMIC_DRAW);

// Update a sub-range
glBufferSubData(GL_ARRAY_BUFFER, offsetBytes, sizeBytes, newData);

// Map directly — zero-copy into GPU memory
void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
memcpy(ptr, newData, sizeof(newData));
glUnmapBuffer(GL_ARRAY_BUFFER);
```

---

## 3.2 VAO — Vertex Array Object

The VAO records how to interpret the data in VBOs:

```cpp
GLuint VAO;
glGenVertexArrays(1, &VAO);
glBindVertexArray(VAO);             // start recording

glBindBuffer(GL_ARRAY_BUFFER, VBO); // which buffer to read from

// Stride = total bytes per vertex = (3 + 3 + 2) * 4 = 32
const int stride = 8 * sizeof(float);

// Attribute 0: position (3 floats, offset 0)
glVertexAttribPointer(0,            // attrib location
                      3,            // component count
                      GL_FLOAT,     // component type
                      GL_FALSE,     // normalize? (for integers)
                      stride,       // bytes between vertices
                      (void*)0);    // byte offset within vertex
glEnableVertexAttribArray(0);

// Attribute 1: normal (3 floats, offset 12 bytes)
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);

// Attribute 2: UV (2 floats, offset 24 bytes)
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
glEnableVertexAttribArray(2);

glBindVertexArray(0);               // stop recording
```

---

## 3.3 EBO — Element Buffer Object (Index Buffer)

Reuse vertices with an index list — saves memory and bandwidth:

```cpp
unsigned int indices[] = {
    0, 1, 2,   // first triangle
    2, 3, 0,   // second triangle (reuses vertices 0 and 2)
};

GLuint EBO;
glGenBuffers(1, &EBO);

// EBO is stored inside the VAO — must bind VAO first
glBindVertexArray(VAO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
glBindVertexArray(0);

// Draw with indices:
glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES,    // primitive type
               6,               // index count
               GL_UNSIGNED_INT, // index type
               0);              // offset into EBO
```

---

## 3.4 Memory Layout Visualization

```
VBO memory (interleaved):
┌──────────────────────────────────────────────────────────────┐
│ V0: [px py pz | nx ny nz | u v] │ V1: [...] │ V2: [...] │...│
│      offset=0    offset=12        offset=24                  │
│      size=12     size=12          size=8                     │
│                                                              │
│                  stride=32 bytes total                       │
└──────────────────────────────────────────────────────────────┘

EBO memory:
┌─────────────────────┐
│ 0│1│2│2│3│0│...    │
└─────────────────────┘
```

---

## 3.5 Textures — Creation and Upload

```cpp
// Loading with stb_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int width, height, channels;
stbi_set_flip_vertically_on_load(true);  // OpenGL UV origin is bottom-left
unsigned char* data = stbi_load("texture.png", &width, &height, &channels, 0);

GLuint texture;
glGenTextures(1, &texture);
glBindTexture(GL_TEXTURE_2D, texture);

// Wrapping: GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

// Filtering
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// Upload texture data
GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
glTexImage2D(GL_TEXTURE_2D,    // target
             0,                // mip level (0 = base)
             GL_RGBA8,         // internal format (GPU storage)
             width, height,
             0,                // border (always 0)
             format,           // source format
             GL_UNSIGNED_BYTE, // source type
             data);

glGenerateMipmap(GL_TEXTURE_2D);  // auto-generate mip chain

stbi_image_free(data);
```

---

## 3.6 Texture Units and Binding

OpenGL has multiple texture **units** (slots) that can be active simultaneously. Each sampler uniform maps to a unit:

```cpp
// Bind textures to units
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, diffuseTexture);

glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, specularTexture);

// Tell the shader which unit each sampler uses
shader.use();
shader.setInt("diffuseMap",  0);  // GL_TEXTURE0
shader.setInt("specularMap", 1);  // GL_TEXTURE1
```

```glsl
// In fragment shader
uniform sampler2D diffuseMap;    // unit 0
uniform sampler2D specularMap;   // unit 1

vec3 diffuse  = texture(diffuseMap,  TexCoord).rgb;
vec3 specular = texture(specularMap, TexCoord).rgb;
```

Minimum 16 texture units guaranteed (GL_MAX_TEXTURE_IMAGE_UNITS, often 32+).

---

## 3.7 Texture Formats

| Internal Format | Channels | Bits/ch | Use Case |
|----------------|----------|---------|----------|
| `GL_RGB8` | 3 | 8 | Standard color (no alpha) |
| `GL_RGBA8` | 4 | 8 | Color with alpha |
| `GL_SRGB8_ALPHA8` | 4 | 8 | sRGB color (correct gamma) |
| `GL_RGB16F` | 3 | 16 float | HDR color buffer |
| `GL_RGBA32F` | 4 | 32 float | High precision / compute |
| `GL_RG16F` | 2 | 16 float | Normal map (G-buffer) |
| `GL_R8` | 1 | 8 | Single channel (AO, roughness) |
| `GL_DEPTH24_STENCIL8` | D+S | 24+8 | Depth+stencil attachment |

---

## 3.8 Cubemap Textures

Six faces for environment maps, skyboxes, reflections:

```cpp
GLuint cubemap;
glGenTextures(1, &cubemap);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

const char* faces[] = {
    "right.jpg","left.jpg","top.jpg","bottom.jpg","front.jpg","back.jpg"
};
for (int i = 0; i < 6; ++i) {
    int w,h,ch;
    auto* d = stbi_load(faces[i], &w, &h, &ch, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
}
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
```

```glsl
// Sample in shader
uniform samplerCube skybox;
vec3 color = texture(skybox, normalize(FragPos)).rgb;
```

---

## 3.9 Anisotropic Filtering

```cpp
// Check support
float maxAniso;
glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);

// Apply (use extension: GL_EXT_texture_filter_anisotropic)
glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
// Or limit to a lower value (4x, 8x) for performance
```

---

## Interview Questions

### Easy

**Q: What is the difference between a VAO and a VBO?**

> **VBO (Vertex Buffer Object)**: a GPU-side buffer that holds raw vertex data (positions, normals, UVs, colors). It's just a blob of bytes on the GPU — no interpretation. **VAO (Vertex Array Object)**: records *how* to interpret the data in one or more VBOs — which attributes are at which byte offsets, how many components each has, what type they are, and which attributes are enabled. The VAO stores the state set by `glVertexAttribPointer` and `glEnableVertexAttribArray`. To draw: bind the VAO, which restores all the VBO binding and attribute configuration in one call. Think of VBO as the data and VAO as the description of that data.

**Q: What is a mipmap and why is it important?**

> A mipmap is a pre-computed sequence of progressively lower-resolution versions of a texture (level 0 = full size, level 1 = half, level 2 = quarter, …). Importance: (1) **Quality**: when a textured surface is viewed from a distance (many texels map to one pixel), sampling without mipmaps causes aliasing (shimmering). Mipmaps avoid this by using a lower-res version appropriate for the viewing distance. (2) **Performance**: GPUs fetch texels in 2×2 blocks for filtering. When using a mip level that fits in cache, fewer cache misses occur. `GL_LINEAR_MIPMAP_LINEAR` (trilinear filtering) blends between two adjacent mip levels for smooth transitions. Always generate mipmaps with `glGenerateMipmap` for minification-filtered textures.

**Q: What is `GL_TEXTURE_WRAP_S` set to `GL_CLAMP_TO_EDGE` vs `GL_REPEAT`?**

> UV coordinates outside [0, 1] are handled differently: `GL_REPEAT`: the texture tiles — UV 1.2 samples the same as UV 0.2. Good for repeating patterns (grass, bricks). `GL_CLAMP_TO_EDGE`: UVs are clamped to [0,1] — anything outside samples the edge pixel. Good for single-use textures (faces, UI elements) where tiling would look wrong. `GL_MIRRORED_REPEAT`: tiles but flips every other tile — reduces seams on some patterns. `GL_CLAMP_TO_BORDER`: samples a configurable border color outside [0,1]. Used in shadow map sampling to return 0 (lit) outside the shadow map.

---

### Mid

**Q: What is the difference between `glTexImage2D` and `glTexStorage2D`?**

> `glTexImage2D` creates one mip level at a time — it may reallocate GPU memory on each call and allows changing the format later. `glTexStorage2D` (OpenGL 4.2+) allocates the **entire mip chain** at once with a fixed, immutable format: `glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, width, height)`. After this, you upload data with `glTexSubImage2D` instead of `glTexImage2D`. Benefits: the driver can allocate optimal memory layouts upfront (tiled/compressed), the texture format is immutable (safer), and some drivers optimize storage. Prefer `glTexStorage2D` in modern code.

**Q: How many textures can a fragment shader use simultaneously?**

> The OpenGL 3.3 minimum guarantee is **16 texture image units** (`GL_MAX_TEXTURE_IMAGE_UNITS`). Modern hardware often provides 32 or more. Each texture unit can have one texture of each type bound simultaneously (one `GL_TEXTURE_2D`, one `GL_TEXTURE_CUBE_MAP`, etc.), but practically you use one sampler per unit. If a material needs more than 16 textures, you either: use texture arrays (`GL_TEXTURE_2D_ARRAY`), bindless textures (OpenGL 4.6 / `ARB_bindless_texture`), or split into multiple draw passes. For a deferred rendering G-buffer, typical usage is 4–8 textures; for PBR materials, 5–7.

---

### Hard

**Q: Explain GPU memory bandwidth considerations when designing vertex buffer layouts.**

> Two layouts: **Array-of-Structures (AoS / interleaved)**: all attributes for one vertex together `[p p p n n n u u | p p p n n n u u]`. Cache-friendly for vertex shaders that access all attributes — one cache line fetch gets the full vertex. **Structure-of-Arrays (SoA / separate buffers)**: one buffer per attribute `[p p p | p p p] [n n n | n n n] [u u | u u]`. Better for compute passes that only need one attribute (e.g., position-only depth pass avoids loading normals/UVs). Modern best practice: use AoS for most rendering. For depth-only passes (shadow maps, early-z), use a separate position-only VBO to minimize fetched data. Align vertex data to 4-byte boundaries. Avoid half-float positions unless memory bandwidth is critical — the conversion cost on some hardware exceeds the bandwidth saving.
