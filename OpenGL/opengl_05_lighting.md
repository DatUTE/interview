# OpenGL 05 — Lighting & Materials

---

## 5.1 Phong Lighting Model

The Phong model approximates local illumination with three components:

```
Final Color = Ambient + Diffuse + Specular
```

| Component | Physical Meaning | Formula |
|-----------|-----------------|---------|
| Ambient | Indirect/global light — prevents total black | `Ka * La` |
| Diffuse | Lambertian: light scattered equally in all directions | `Kd * Ld * max(dot(N,L), 0)` |
| Specular | Mirror-like highlights | `Ks * Ls * pow(max(dot(R,V), 0), shininess)` |

Where: N = surface normal, L = light direction, V = view direction, R = reflected light direction.

---

## 5.2 Phong Fragment Shader

```glsl
#version 330 core

in vec3 FragPos;   // world-space position
in vec3 Normal;    // world-space normal
in vec2 TexCoord;

out vec4 FragColor;

// Material
struct Material {
    sampler2D diffuse;   // diffuse color / albedo map
    sampler2D specular;  // specular intensity map
    float     shininess;
};

// Light
struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material material;
uniform Light    light;
uniform vec3     viewPos;

void main() {
    vec3 albedo  = texture(material.diffuse,  TexCoord).rgb;
    vec3 specMap = texture(material.specular, TexCoord).rgb;

    vec3 N = normalize(Normal);
    vec3 L = normalize(light.position - FragPos);
    vec3 V = normalize(viewPos        - FragPos);
    vec3 R = reflect(-L, N);

    // Ambient
    vec3 ambient  = light.ambient  * albedo;

    // Diffuse
    float diff    = max(dot(N, L), 0.0);
    vec3  diffuse = light.diffuse  * diff * albedo;

    // Specular (Phong)
    float spec    = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specMap;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

---

## 5.3 Blinn-Phong (Improved Specular)

Replace `dot(V, R)` with `dot(N, H)` where `H = normalize(L + V)` (halfway vector):

```glsl
vec3 H    = normalize(L + V);
float spec = pow(max(dot(N, H), 0.0), material.shininess);
```

**Advantages of Blinn-Phong:**
- No artifact when viewing angle > 90° from reflection direction
- More physically plausible energy distribution
- Slightly cheaper to compute (avoids `reflect()`)
- Standard in most real-time renderers

---

## 5.4 Light Types

### Directional Light (Sun)
```glsl
struct DirLight {
    vec3 direction;   // all rays parallel — no position
    vec3 ambient, diffuse, specular;
};

vec3 L = normalize(-light.direction);  // negate: light.direction = sun→world
```

### Point Light (Bulb)
```glsl
struct PointLight {
    vec3 position;
    vec3 ambient, diffuse, specular;

    // Attenuation: 1 / (Kc + Kl*d + Kq*d^2)
    float constant;   // Kc = 1.0
    float linear;     // Kl = 0.09 (for range ~50 units)
    float quadratic;  // Kq = 0.032
};

float d = length(light.position - FragPos);
float attenuation = 1.0 / (light.constant + light.linear * d + light.quadratic * d * d);
```

### Spot Light (Flashlight)
```glsl
struct SpotLight {
    vec3 position, direction;
    float cutoff;       // cos of inner angle
    float outerCutoff;  // cos of outer angle (soft edge)
    // + attenuation params + colors
};

float theta   = dot(L, normalize(-light.direction));
float epsilon = light.cutoff - light.outerCutoff;
float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
// Multiply diffuse + specular by intensity
```

---

## 5.5 Multiple Lights

```glsl
// Up to N point lights
#define MAX_POINT_LIGHTS 4

uniform int          numLights;
uniform PointLight   pointLights[MAX_POINT_LIGHTS];
uniform DirLight     dirLight;
uniform SpotLight    spotLight;

vec3 calc_dir_light (DirLight l,   vec3 N, vec3 V) { ... }
vec3 calc_point_light(PointLight l, vec3 N, vec3 V, vec3 pos) { ... }
vec3 calc_spot_light(SpotLight l,  vec3 N, vec3 V, vec3 pos) { ... }

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 result = calc_dir_light(dirLight, N, V);
    for (int i = 0; i < numLights; i++)
        result += calc_point_light(pointLights[i], N, V, FragPos);
    result += calc_spot_light(spotLight, N, V, FragPos);
    FragColor = vec4(result, 1.0);
}
```

---

## 5.6 PBR — Physically Based Rendering

Modern alternative to Phong — energy-conserving, artist-friendly parameters:

```glsl
// Cook-Torrance BRDF (simplified)
// Parameters: albedo, metallic, roughness, ao (ambient occlusion)

uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

// Distribution term (GGX/Trowbridge-Reitz)
float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Fresnel (Schlick approximation)
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Geometry (Smith's method)
float G_Smith(float NdotV, float NdotL, float k) {
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

void main() {
    // ... compute N, V, L, H
    vec3 F0 = mix(vec3(0.04), albedo, metallic);  // base reflectivity

    float D = D_GGX(max(dot(N,H), 0.0), roughness * roughness);
    vec3  F = F_Schlick(max(dot(H,V), 0.0), F0);
    float G = G_Smith(max(dot(N,V),0), max(dot(N,L),0), roughness);

    vec3 numerator   = D * F * G;
    float denominator = 4.0 * max(dot(N,V),0) * max(dot(N,L),0) + 0.0001;
    vec3 specular    = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * max(dot(N,L), 0.0);
    vec3 color = Lo + vec3(0.03) * albedo * ao;
    FragColor = vec4(color, 1.0);
}
```

---

## 5.7 PBR Texture Maps

| Map | Channels | Meaning |
|-----|----------|---------|
| Albedo | RGB | Base color |
| Normal | RGB | Surface micro-normals (tangent space) |
| Metallic | R | 0 = dielectric, 1 = metal |
| Roughness | R | 0 = mirror, 1 = completely rough |
| AO | R | Ambient occlusion (shadowed areas) |
| Emissive | RGB | Self-illuminated color |
| Height/Displacement | R | For parallax/tessellation |

---

## Interview Questions

### Easy

**Q: What is the ambient component in Phong lighting and why is it needed?**

> In real life, light bounces off many surfaces before reaching a point, filling in areas that are not directly lit. The Phong model approximates this with a constant ambient term — a small fraction of the light color added regardless of surface orientation. Without it, surfaces facing away from the light would be completely black, which looks unrealistic. The ambient term is a cheap but crude approximation of **global illumination**. More accurate alternatives: ambient occlusion maps (baked or SSAO), image-based lighting (IBL with environment maps), or real-time global illumination (Lumen, DDGI).

**Q: What is the difference between per-vertex lighting (Gouraud) and per-fragment lighting (Phong)?**

> **Gouraud shading**: lighting computed once per vertex in the vertex shader. Colors are then interpolated across the triangle during rasterization. Fast but misses specular highlights that fall inside a triangle (they get "averaged away"). Good for diffuse-heavy scenes. **Phong shading**: normals are interpolated per vertex, but lighting is computed for every fragment in the fragment shader. Captures specular highlights correctly even on large low-poly triangles. All modern real-time rendering uses per-fragment lighting. The performance cost is higher (many more lighting evaluations), but GPU fragment throughput is very high, making it the standard approach.

**Q: What is attenuation in point lights and why is quadratic the standard formula?**

> Attenuation defines how light intensity decreases with distance. Physically correct: `1 / d²` (inverse square law). In practice, a constant is added to avoid a singularity at d=0 and a linear term for artistic control: `attenuation = 1 / (Kc + Kl*d + Kq*d²)`. At close range, the constant `Kc` dominates. At medium range, `Kl*d` fades it. At far range, `Kq*d²` provides a steep cutoff. Purely quadratic gives too dark a falloff at typical game distances. The constants are tuned per light (Ogre3D and LearnOpenGL publish tables for different effective ranges). The denominator is NOT physically derived — it's a game engine approximation that looks good and artists can tune.

---

### Mid

**Q: What is the difference between Phong and Blinn-Phong specular? When does Phong fail?**

> Phong specular uses `pow(dot(R, V), shininess)` where R is the reflection of L around N. **Failure case**: when V and L are on the same side (e.g., grazing angles or very large surfaces), the angle between R and V can exceed 90°, giving `dot(R,V) < 0`, which `max(..., 0)` clips to 0 — abrupt cutoff. Blinn-Phong uses `pow(dot(N, H), shininess)` where `H = normalize(L+V)` (halfway vector). The angle between N and H is always ≤ 90° for surfaces facing the light, so no cutoff artifact. Blinn-Phong also looks more physically plausible at grazing angles and matches real-world measurements better for many materials. To get the same apparent highlight size, Blinn-Phong needs a higher shininess value (~4x).

**Q: Why do normal maps use tangent space rather than world space?**

> **World-space normal maps**: store normals in world coordinates — work correctly, but the map is baked for a specific object orientation. Rotating the object requires a different map. **Tangent-space normal maps**: store normals relative to the surface — the "default" normal (0,0,1) points straight out from the surface. This is orientation-independent: the same map works on any rotation or mirror of the object. The GPU transforms the tangent-space normals to world space using the TBN matrix (Tangent, Bitangent, Normal) computed per-vertex. Tangent-space normals typically look mostly blue (most normals close to the surface, encoded as (0.5,0.5,1.0)). They're reusable across mirrored UV layouts, which saves texture memory.

---

### Hard

**Q: Implement a complete PBR material system with IBL (Image-Based Lighting) ambient term.**

> IBL replaces the flat ambient term with environment-based illumination — the indirect light contribution from the surrounding scene: ```glsl // Precomputed maps (offline): // 1. irradianceMap: diffuse IBL — convolved cubemap (average hemisphere) // 2. prefilterMap: specular IBL — multi-mip cubemap (different roughness per mip) // 3. brdfLUT: BRDF integration map (2D texture: NdotV, roughness → scale,bias)

uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;

// Ambient IBL:
vec3 kS = F_Schlick_roughness(max(dot(N,V), 0.0), F0, roughness);
vec3 kD = (1.0 - kS) * (1.0 - metallic);

// Diffuse IBL
vec3 irradiance = texture(irradianceMap, N).rgb;
vec3 diffuse_ibl = irradiance * albedo * kD;

// Specular IBL (split-sum approximation: Epic Games)
const float MAX_REFLECTION_LOD = 4.0;
vec3 prefilteredColor = textureLod(prefilterMap, reflect(-V, N),
                                   roughness * MAX_REFLECTION_LOD).rgb;
vec2 brdf = texture(brdfLUT, vec2(max(dot(N,V), 0.0), roughness)).rg;
vec3 specular_ibl = prefilteredColor * (F0 * brdf.x + brdf.y);

vec3 ambient = (diffuse_ibl + specular_ibl) * ao;
vec3 color = ambient + Lo;  // Lo = direct lighting sum``` The offline precomputation (irradiance convolution, specular pre-filter, BRDF LUT) is done once. At runtime, three texture lookups replace the full environment integral — this is the split-sum approximation from Unreal Engine's 2013 Siggraph talk.
