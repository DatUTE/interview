# OpenGL 06 — Framebuffers, Blending & Stencil

---

## 6.1 Framebuffer Objects (FBO)

An FBO is a user-created off-screen render target — render to a texture, not the screen:

```cpp
// Create FBO
GLuint FBO;
glGenFramebuffers(1, &FBO);
glBindFramebuffer(GL_FRAMEBUFFER, FBO);

// Attach a color texture
GLuint colorTex;
glGenTextures(1, &colorTex);
glBindTexture(GL_TEXTURE_2D, colorTex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

// Attach a depth+stencil renderbuffer
GLuint RBO;
glGenRenderbuffers(1, &RBO);
glBindRenderbuffer(GL_RENDERBUFFER, RBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

// Verify
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "FBO not complete!\n";

glBindFramebuffer(GL_FRAMEBUFFER, 0);  // unbind — back to default
```

---

## 6.2 Rendering to a Texture (Render-to-Texture)

```cpp
// ── Render scene to FBO ──────────────────────────────────
glBindFramebuffer(GL_FRAMEBUFFER, FBO);
glViewport(0, 0, FBOWidth, FBOHeight);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
// ... render scene normally ...

// ── Use the texture in a post-process pass ───────────────
glBindFramebuffer(GL_FRAMEBUFFER, 0);  // default framebuffer (screen)
glViewport(0, 0, windowWidth, windowHeight);
glClear(GL_COLOR_BUFFER_BIT);

postProcessShader.use();
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, colorTex);  // the rendered scene
// Draw a full-screen quad...
```

**Screen-space full-screen quad:**
```cpp
float quadVerts[] = {
    // pos (x,y)  // uv
    -1.0f,  1.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};
// No MVP needed — vertices already in NDC
```

---

## 6.3 Post-Processing Shaders

```glsl
// Post-process fragment shader template
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D screenTex;

void main() {
    vec2 texelSize = 1.0 / textureSize(screenTex, 0);
    vec3 col = texture(screenTex, TexCoord).rgb;

    // Invert
    // FragColor = vec4(1.0 - col, 1.0);

    // Grayscale
    // float gray = dot(col, vec3(0.2126, 0.7152, 0.0722));  // luminance weights
    // FragColor = vec4(vec3(gray), 1.0);

    // Kernel convolution (e.g., Gaussian blur, edge detect, sharpen)
    float kernel[9] = float[](
        1,  2, 1,
        2,  4, 2,   // Gaussian 3x3 (sum=16, divide=1/16)
        1,  2, 1
    );
    vec3 result = vec3(0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            result += texture(screenTex, TexCoord + vec2(x,y)*texelSize).rgb
                    * kernel[(y+1)*3+(x+1)];
    FragColor = vec4(result / 16.0, 1.0);
}
```

---

## 6.4 Depth Testing

```cpp
// Enable — always on for 3D rendering
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LESS);       // pass if incoming < stored (default)
// Other: GL_LEQUAL, GL_GREATER (for reverse-Z), GL_ALWAYS, GL_NEVER

// Read-only depth (for transparent objects, skybox)
glDepthMask(GL_FALSE);      // don't write to depth buffer
// ... draw transparent/skybox ...
glDepthMask(GL_TRUE);

// Visualize depth buffer (in shader)
float depth = gl_FragCoord.z;
// Non-linear (perspective) — linearize:
float near = 0.1, far = 100.0;
float linear = (2.0 * near * far) / (far + near - depth * (far - near));
FragColor = vec4(vec3(linear / far), 1.0);
```

---

## 6.5 Stencil Buffer

The stencil buffer controls per-pixel write/discard based on a reference value:

```cpp
glEnable(GL_STENCIL_TEST);

// ── Pass 1: Write to stencil ─────────────────────────────
glStencilFunc(GL_ALWAYS, 1, 0xFF);  // always pass stencil test, ref=1
glStencilOp(GL_KEEP,     // stencil fail → keep
            GL_KEEP,     // depth fail → keep
            GL_REPLACE); // both pass → write ref=1
glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // don't write color
glDepthMask(GL_FALSE);
// Draw the "mask" object (fills stencil where object is)

// ── Pass 2: Render only where stencil == 1 ──────────────
glStencilFunc(GL_EQUAL, 1, 0xFF);   // pass only if stored == 1
glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); // don't change stencil
glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
glDepthMask(GL_TRUE);
// Draw effect (only visible through "stencil mask")

// ── Object outline via stencil ───────────────────────────
// 1. Write stencil for object at normal scale
// 2. Draw slightly scaled object with stencil test GL_NOTEQUAL
//    → only ring outside original is visible
```

---

## 6.6 Blending

```cpp
glEnable(GL_BLENDING);
// finalColor = srcFactor * srcColor + dstFactor * dstColor
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // standard alpha blend
glBlendEquation(GL_FUNC_ADD);  // default

// Common blend modes:
// Alpha blend:   GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
// Additive:      GL_SRC_ALPHA, GL_ONE           (particles, glow)
// Multiplicative: GL_DST_COLOR, GL_ZERO
// Pre-multiplied: GL_ONE, GL_ONE_MINUS_SRC_ALPHA

// Transparent objects MUST be sorted back-to-front!
// Otherwise: closer transparent object blended before farther → wrong order
std::sort(transparentObjects.begin(), transparentObjects.end(),
    [&camera](const auto& a, const auto& b) {
        return glm::distance(camera.Position, a.position) >
               glm::distance(camera.Position, b.position);
    });
```

---

## 6.7 Deferred Rendering

Traditional forward rendering: per light × per object cost. Deferred: separate geometry pass from lighting pass.

```
Pass 1 — Geometry Pass (write G-buffer):
  FBO with 3–4 color attachments:
  ┌──────────┬──────────────┬────────────┬──────────┐
  │ Position │ Normal (RG16F)│ Albedo+AO  │ Emission │
  └──────────┴──────────────┴────────────┴──────────┘

Pass 2 — Lighting Pass (fullscreen quad, read G-buffer):
  for each light:
      read Position, Normal, Albedo from G-buffer textures
      compute lighting → accumulate to HDR buffer

Pass 3 — Post-process (tone mapping, bloom, etc.)
```

```cpp
// G-buffer setup: multiple color attachments
GLuint attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
glDrawBuffers(3, attachments);
```

---

## Interview Questions

### Easy

**Q: What is an FBO and when do you need one?**

> An FBO (Framebuffer Object) is a user-created render target — instead of rendering to the window's default framebuffer (the screen), you render into textures attached to the FBO. You need one for: post-processing effects (blur, bloom, tone mapping — render scene to texture, apply effect, display result); shadow maps (render from light's perspective); G-buffer in deferred rendering; reflection/refraction (render scene from reflected view to texture); any "render-to-texture" technique. The default framebuffer (ID=0) is provided by the windowing system (GLFW/SDL) and cannot be configured. FBOs give full control over attachments, formats, and sizes.

**Q: What is the painter's algorithm and why is it insufficient for alpha blending?**

> The painter's algorithm sorts objects back-to-front and draws them in that order — like a painter who first paints the background, then foreground objects on top. For opaque objects, the depth buffer is more efficient. For **transparent objects**: depth testing doesn't work (you can't just skip transparent fragments that fail depth test — they need to blend with what's behind them). The painter's algorithm handles this correctly in theory: draw transparent objects from back to front so each blends with correctly rendered content. It fails when: objects intersect (no single correct sort order), cyclic dependencies exist (A in front of B, B in front of C, C in front of A), or objects are not convex. Solutions: OIT (Order-Independent Transparency), depth peeling, weighted blended OIT.

**Q: What is `glDepthMask(GL_FALSE)` and when is it used?**

> `glDepthMask(GL_FALSE)` makes the depth buffer **read-only** — fragments can still be tested against depth (discarding occluded fragments), but passing fragments don't write their depth to the buffer. Use cases: (1) **Transparent objects**: they should be tested against opaque depth but shouldn't occlude each other or other transparent objects via depth writes. (2) **Skybox**: draw last, test against depth (skip where objects are) but don't write depth (the skybox is "infinitely far"). (3) **Particle systems**: particles test against scene depth but don't write, avoiding odd Z-fighting between particles.

---

### Mid

**Q: Explain deferred rendering vs forward rendering. When would you choose each?**

> **Forward rendering**: for each object, iterate over all lights and accumulate lighting in one pass. Cost: O(objects × lights) draw calls. Problem: 100 objects × 100 lights = 10,000 per-fragment lighting computations. Works well with few lights (<10). Simple to implement, works with MSAA natively, handles transparency naturally. **Deferred rendering**: geometry pass fills a G-buffer (position, normal, albedo) in one draw call per object. Lighting pass reads the G-buffer and applies each light in screen space — only lit pixels are shaded once. Cost: O(objects) for geometry + O(lights × screen_pixels) for lighting. Handles hundreds of lights efficiently. Problems: high memory bandwidth (G-buffer reads), no hardware MSAA (requires manual resolve), transparent objects require forward pass. **Choose forward** for: few lights, many transparent objects, VR (MSAA critical). **Choose deferred** for: many dynamic lights, large open scenes.

**Q: How does the stencil buffer enable an object outline effect?**

> Outline technique using two passes: Pass 1 — render the object normally, but also write `1` to the stencil buffer wherever the object's fragments pass both stencil (set to `GL_ALWAYS`) and depth tests (`glStencilOp(KEEP, KEEP, GL_REPLACE)`). Pass 2 — render the same object again, slightly scaled up (push vertices along normals, or just use a scale matrix). Set stencil test to `GL_NOTEQUAL, 1, 0xFF` — draw only where stencil is NOT 1. Since the scaled-up version extends slightly past the original, only the "ring" outside the original object passes the stencil test, creating an outline. The outline shader outputs a flat color (e.g., white or bright orange). This is how object selection highlights work in many game engines.

---

### Hard

**Q: Implement multisampling anti-aliasing (MSAA) with a custom FBO.**

> ```cpp
> // Create multisampled FBO
> GLuint MSFBO, MSColorRB, MSDepthRB;
> int samples = 4;  // 4× MSAA
>
> glGenFramebuffers(1, &MSFBO);
> glBindFramebuffer(GL_FRAMEBUFFER, MSFBO);
>
> // Multisampled renderbuffers (not textures — no filtering)
> glGenRenderbuffers(1, &MSColorRB);
> glBindRenderbuffer(GL_RENDERBUFFER, MSColorRB);
> glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
> glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, MSColorRB);
>
> glGenRenderbuffers(1, &MSDepthRB);
> glBindRenderbuffer(GL_RENDERBUFFER, MSDepthRB);
> glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
> glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, MSDepthRB);
>
> // Regular FBO for resolved result (post-process reads this)
> GLuint resolveFBO, resolveColorTex;
> // ... create resolveColorTex (regular GL_TEXTURE_2D) ...
>
> // Render to MSFBO normally, then resolve:
> glBindFramebuffer(GL_READ_FRAMEBUFFER, MSFBO);
> glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
> glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
>                   GL_COLOR_BUFFER_BIT, GL_NEAREST);
> // resolveColorTex now contains the anti-aliased result
> ```
> Note: deferred rendering can't use MSAA this way because G-buffer textures can't be multisampled in a way that makes lighting sense. Solutions: SMAA/TAA in post-processing, or per-sample shading (`GL_SAMPLE_SHADING`) at high cost.
