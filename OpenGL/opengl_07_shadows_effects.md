# OpenGL 07 — Shadows, Normal Maps & HDR

---

## 7.1 Shadow Mapping

Two-pass technique: render from light's POV to a depth texture, then test scene fragments against it.

### Pass 1 — Shadow Map Generation

```glsl
// depth_vert.glsl — render scene from light's view
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 lightSpaceMatrix;  // light's projection * view
uniform mat4 model;
void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
// Fragment shader: empty (only depth written)
```

```cpp
// Setup shadow map FBO
GLuint shadowFBO, shadowMap;
const int SHADOW_W = 2048, SHADOW_H = 2048;

glGenFramebuffers(1, &shadowFBO);
glGenTextures(1, &shadowMap);
glBindTexture(GL_TEXTURE_2D, shadowMap);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOW_W, SHADOW_H, 0,
             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = {1,1,1,1};  // outside shadow map = lit
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

// Enable hardware PCF (percentage closer filtering)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);

// Light space matrix
glm::mat4 lightProj = glm::ortho(-10.f, 10.f, -10.f, 10.f, 1.f, 50.f);
glm::mat4 lightView = glm::lookAt(lightDir * 20.f, glm::vec3(0), glm::vec3(0,1,0));
glm::mat4 lightSpaceMat = lightProj * lightView;
```

### Pass 2 — Shadow Test in Fragment Shader

```glsl
// scene_frag.glsl (shadow sampling)
in vec4 FragPosLightSpace;  // passed from vertex shader
uniform sampler2DShadow shadowMap;

float shadow_test(vec4 fragPosLS) {
    // Perspective divide → NDC
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords = projCoords * 0.5 + 0.5;  // [−1,1] → [0,1]

    if (projCoords.z > 1.0) return 0.0;  // beyond far plane → lit

    // PCF — sample 3×3 neighborhood
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            shadow += texture(shadowMap, vec3(projCoords.xy + vec2(x,y)*texelSize, projCoords.z));
    return shadow / 9.0;  // 0=shadowed, 1=lit
}
```

---

## 7.2 Normal Mapping

Store per-texel surface normals in a texture to fake micro-surface detail:

### Vertex Shader — Compute TBN Matrix

```glsl
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;       // tangent space → world space transform
} vs_out;

uniform mat4 model, view, projection;
uniform mat3 normalMatrix;

void main() {
    vs_out.FragPos   = vec3(model * vec4(aPos, 1.0));
    vs_out.TexCoord  = aTexCoord;

    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);
    vec3 N = normalize(normalMatrix * aNormal);
    vs_out.TBN = mat3(T, B, N);  // columns

    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
```

### Fragment Shader — Apply Normal Map

```glsl
in VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
} fs_in;

uniform sampler2D normalMap;

void main() {
    // Sample normal map: [0,1] → [−1,1]
    vec3 normal = texture(normalMap, fs_in.TexCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);  // tangent space

    // Transform to world space
    vec3 N = normalize(fs_in.TBN * normal);

    // Proceed with lighting using N
    // ...
}
```

---

## 7.3 Parallax Occlusion Mapping

Simulates depth by offsetting UVs based on view direction and a height map:

```glsl
// Basic parallax (cheap — single sample)
vec2 parallax_uv(vec2 uv, vec3 viewDirTangent, sampler2D heightMap) {
    float height = texture(heightMap, uv).r;
    vec2 offset = viewDirTangent.xy / viewDirTangent.z * height * 0.1;
    return uv - offset;
}

// Parallax Occlusion Mapping — ray-march into height field
vec2 parallax_occlusion(vec2 uv, vec3 viewDirT, sampler2D heightMap) {
    const int   steps  = 16;
    const float depth  = 0.1;  // max depth
    float stepSize = depth / steps;
    vec2  deltaUV  = viewDirT.xy / viewDirT.z * depth / steps;

    float curDepth  = 0.0;
    float mapDepth  = 1.0;

    while (curDepth < mapDepth) {
        uv      -= deltaUV;
        mapDepth = texture(heightMap, uv).r;
        curDepth += stepSize;
    }
    // Interpolate between last two samples
    vec2  prevUV    = uv + deltaUV;
    float prevDepth = texture(heightMap, prevUV).r;
    float frac = (curDepth - mapDepth) / (curDepth - prevDepth + 0.0001);
    return mix(uv, prevUV, frac);
}
```

---

## 7.4 HDR & Tone Mapping

High Dynamic Range (HDR) rendering — use floating-point buffers to avoid clamping bright values, then tone-map to LDR for display:

```glsl
// HDR framebuffer: GL_RGB16F or GL_R11F_G11F_B10F
// Tone mapping fragment shader (fullscreen pass)

uniform sampler2D hdrBuffer;
uniform float exposure;

void main() {
    vec3 hdrColor = texture(hdrBuffer, TexCoord).rgb;

    // Reinhard tone mapping
    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));

    // Exposure-based
    vec3 mapped2 = vec3(1.0) - exp(-hdrColor * exposure);

    // ACES filmic (industry standard)
    vec3 x = hdrColor;
    float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    vec3 aces = clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);

    // Gamma correction (sRGB: gamma 2.2)
    vec3 final = pow(aces, vec3(1.0 / 2.2));

    FragColor = vec4(final, 1.0);
}
```

---

## 7.5 Bloom

Extract bright areas, blur them, add back to scene:

```
Scene → HDR buffer
              ↓
         Bright pass (threshold)
              ↓
         Gaussian blur (pingpong FBOs)
              ↓
         Combine: HDR + blurred bright
              ↓
         Tone map → screen
```

```glsl
// Bright pass: extract pixels above threshold
void main() {
    vec3 col = texture(hdrBuffer, TexCoord).rgb;
    float brightness = dot(col, vec3(0.2126, 0.7152, 0.0722));
    FragColor = (brightness > 1.0) ? vec4(col, 1.0) : vec4(0,0,0,1);
}

// Gaussian blur: ping-pong between two FBOs
// horizontal pass: blur along X, vertical pass: blur along Y
```

---

## 7.6 SSAO — Screen Space Ambient Occlusion

Approximate ambient occlusion in screen space using depth and normals:

```glsl
// 1. Sample hemisphere of points around fragment in view space
// 2. For each sample: project to screen, compare depth to G-buffer depth
// 3. Fraction of occluded samples = AO factor

uniform sampler2D gPosition;   // view-space position
uniform sampler2D gNormal;     // view-space normal
uniform sampler2D texNoise;    // 4x4 random rotation vectors
uniform vec3 samples[64];      // hemisphere samples

const vec2 noiseScale = vec2(screenWidth/4.0, screenHeight/4.0);

void main() {
    vec3 fragPos = texture(gPosition, TexCoord).xyz;
    vec3 normal  = normalize(texture(gNormal, TexCoord).rgb);
    vec3 randVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);

    // TBN to orient hemisphere along normal
    vec3 tangent  = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float radius = 0.5;
    for (int i = 0; i < 64; i++) {
        vec3 samplePos = TBN * samples[i];  // view space
        samplePos = fragPos + samplePos * radius;

        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz  = (offset.xyz / offset.w) * 0.5 + 0.5;  // screen UV

        float sampleDepth = texture(gPosition, offset.xy).z;
        float rangeCheck  = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + 0.025 ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / 64.0);
    FragColor = vec4(occlusion, occlusion, occlusion, 1.0);
    // Blur the SSAO output (3x3 or bilateral) to reduce noise
}
```

---

## Interview Questions

### Easy

**Q: What is shadow acne and how do you fix it?**

> Shadow acne (self-shadowing artifact): a surface incorrectly shadows itself because the shadow map's stored depth and the actual surface depth are the same value with floating-point precision errors — causing a mottled pattern of lit/shadowed fragments on the same surface. **Fixes**: (1) **Shadow bias**: add a small offset `projCoords.z -= 0.005` (or adaptive bias based on the angle between N and L: `bias = max(0.05 * (1.0 - dot(N,L)), 0.005)`). Reduces self-shadowing but may cause "Peter Panning" (shadow detaches from object). (2) **Front-face culling** during shadow map generation: render only back faces into the shadow map — shifts the depth to the back surface. (3) **Higher resolution shadow map**: reduces the world area covered by a single texel.

**Q: What is PCF (Percentage Closer Filtering) and how does it soften shadows?**

> PCF is a technique to get soft shadows from a shadow map without pre-filtering the depth values. Instead of sampling the shadow map once, sample a neighborhood (e.g., 3×3 = 9 samples) around the projected coordinate, each returning 0 (shadowed) or 1 (lit) via the depth comparison. Average the results. A fragment partially covered by the shadow boundary gets a value between 0 and 1 — a soft shadow edge. Critically: you filter the **comparison results**, not the depth values themselves (filtering depths directly would give wrong shadows). OpenGL supports hardware PCF with `sampler2DShadow` and bilinear interpolation of the comparison results for free (using `GL_TEXTURE_COMPARE_MODE`).

**Q: What is gamma correction and why is it important?**

> Monitors display brightness non-linearly — they apply gamma ≈ 2.2 (darken mid-tones). sRGB textures are stored pre-corrected for this (brighter than they appear). Without gamma correction in your pipeline: textures need to be linearized before lighting math (multiply by albedo), and the final result needs to be gamma-encoded before display. If you do lighting in gamma space, the math is wrong (energy not conserved correctly). Fix: (1) Mark albedo/diffuse textures as `GL_SRGB8_ALPHA8` — the GPU automatically linearizes on sample. (2) At the end of your pipeline, raise the result to power `1/2.2`: `fragColor = pow(color, vec3(1.0/2.2))`. Or use an sRGB framebuffer: `glEnable(GL_FRAMEBUFFER_SRGB)` — automatic gamma encoding to the screen.

---

### Mid

**Q: Explain the difference between shadow mapping artifacts: acne, Peter Panning, and perspective aliasing.**

> **Shadow acne**: surface self-shadows itself due to depth precision. Fix: bias. **Peter Panning**: the bias is too large, causing shadows to detach from their casters — objects appear to float. Fix: reduce bias or use back-face culling during shadow pass. **Perspective aliasing** (shadow pixelation): a single shadow map texel covers a large area of the scene (especially near the camera). Far objects can look fine while near objects have blocky shadows. Fix: **Cascaded Shadow Maps (CSM)** — use multiple shadow maps at different distances with different frustums. Close cascade has fine detail; far cascade covers wide area at coarser detail. Typically 3–4 cascades. The fragment shader selects the appropriate cascade based on the fragment's view-space depth.

---

### Hard

**Q: Describe the full shadow mapping implementation for a directional light with CSM (Cascaded Shadow Maps).**

> CSM splits the view frustum into N sub-frustums (cascades) at increasing depths. For each cascade: (1) Compute the frustum corners in world space for that depth range. (2) Fit an orthographic projection around those corners (with some padding) — compute the min/max of corners projected into light space. (3) Round to texel size to prevent shadow "swimming" as the camera moves. (4) Render the scene into a separate shadow map (or a layer of a texture array) for that cascade. At render time: determine which cascade the fragment falls into by comparing view-space depth to the cascade split depths. Sample the appropriate shadow map. Blend at cascade boundaries (fade between last two) to avoid visible seams. Typical setup: cascades at 5m, 20m, 80m, 300m — near cascade 512×512 (covers only close area with fine detail), far cascade 2048×2048 (large area, coarser). CSM is the standard shadow technique in AAA real-time rendering.
