# OpenGL 04 — Transformations & Camera

---

## 4.1 GLM — OpenGL Mathematics Library

Header-only C++ math library that mirrors GLSL types:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

glm::vec3 pos(1.0f, 2.0f, 3.0f);
glm::vec4 col(1.0f, 0.0f, 0.0f, 1.0f);
glm::mat4 identity(1.0f);  // identity matrix

// Send to shader
glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(matrix));
```

---

## 4.2 Transformation Matrices

```cpp
// Translation — move by (tx, ty, tz)
glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));

// Rotation — rotate 45° around Y axis
glm::mat4 R = glm::rotate(glm::mat4(1.0f),
                           glm::radians(45.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f));

// Scale — scale by (2, 2, 2)
glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));

// Combine: TRS — apply scale first, then rotate, then translate
// Matrix multiplication is right-to-left: T * R * S applies S first
glm::mat4 model = T * R * S;

// Transform a point
glm::vec4 worldPos = model * glm::vec4(localPos, 1.0f);
// Transform a direction (w=0 means not affected by translation)
glm::vec4 worldDir = model * glm::vec4(localDir, 0.0f);
```

---

## 4.3 The MVP Transform Chain

```
Object Space ──[Model]──► World Space ──[View]──► Camera Space ──[Projection]──► Clip Space
```

```glsl
// In vertex shader
gl_Position = projection * view * model * vec4(aPos, 1.0);
```

```cpp
// Model: positions the object in the world
glm::mat4 model = glm::translate(glm::mat4(1.0f), objectPosition);

// View: moves the world so the camera is at the origin
glm::mat4 view = glm::lookAt(
    cameraPos,              // eye position
    cameraPos + cameraFront,// look-at target
    cameraUp                // world up vector
);

// Projection: perspective or orthographic
glm::mat4 projection = glm::perspective(
    glm::radians(45.0f),  // field of view (vertical)
    (float)width / height, // aspect ratio
    0.1f,                  // near plane
    100.0f                 // far plane
);

glm::mat4 MVP = projection * view * model;
```

---

## 4.4 Perspective vs Orthographic Projection

```cpp
// Perspective — objects farther away appear smaller (3D look)
glm::mat4 persp = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

// Orthographic — no perspective foreshortening (engineering/UI)
// Parameters: left, right, bottom, top, near, far
glm::mat4 ortho = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
// For 2D rendering (window coords 0..width, 0..height)
glm::mat4 ui_proj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
```

**Frustum:**
```
         top
        /   \
       /     \
near ──────── far
       \     /
        \   /
         bottom

Near and far planes define the visible range.
Outside → clipped. Too small near → Z-fighting.
```

---

## 4.5 Camera Class (Euler Angles)

```cpp
class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;

    float Yaw   = -90.0f;  // -90° = looking forward (−Z)
    float Pitch =   0.0f;
    float Fov   =  45.0f;
    float Speed =   5.0f;
    float Sensitivity = 0.1f;

    Camera(glm::vec3 pos = {0,0,3}) : Position(pos) {
        Up = glm::vec3(0,1,0);
        update_vectors();
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // WASD movement
    void ProcessKeyboard(int key, float dt) {
        float v = Speed * dt;
        if (key == GLFW_KEY_W) Position += Front * v;
        if (key == GLFW_KEY_S) Position -= Front * v;
        if (key == GLFW_KEY_A) Position -= Right * v;
        if (key == GLFW_KEY_D) Position += Right * v;
    }

    // Mouse look
    void ProcessMouse(float xoffset, float yoffset) {
        Yaw   += xoffset * Sensitivity;
        Pitch += yoffset * Sensitivity;
        Pitch  = glm::clamp(Pitch, -89.0f, 89.0f);
        update_vectors();
    }

    void ProcessScroll(float yoffset) {
        Fov = glm::clamp(Fov - yoffset, 1.0f, 90.0f);
    }

private:
    void update_vectors() {
        glm::vec3 dir;
        dir.x = cos(glm::radians(Yaw))   * cos(glm::radians(Pitch));
        dir.y = sin(glm::radians(Pitch));
        dir.z = sin(glm::radians(Yaw))   * cos(glm::radians(Pitch));
        Front = glm::normalize(dir);
        Right = glm::normalize(glm::cross(Front, glm::vec3(0,1,0)));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};
```

---

## 4.6 View Frustum Culling

Don't submit geometry that's outside the camera's view frustum:

```cpp
struct Frustum {
    glm::vec4 planes[6];  // {normal.xyz, -dot(normal, point)}

    // Extract from MVP matrix (Gribb-Hartmann method)
    void extract(const glm::mat4& mvp) {
        // Left = row3 + row0
        planes[0] = glm::row(mvp, 3) + glm::row(mvp, 0);
        // Right = row3 - row0
        planes[1] = glm::row(mvp, 3) - glm::row(mvp, 0);
        // Bottom, Top, Near, Far...
        planes[2] = glm::row(mvp, 3) + glm::row(mvp, 1);
        planes[3] = glm::row(mvp, 3) - glm::row(mvp, 1);
        planes[4] = glm::row(mvp, 3) + glm::row(mvp, 2);
        planes[5] = glm::row(mvp, 3) - glm::row(mvp, 2);
        for (auto& p : planes)
            p /= glm::length(glm::vec3(p));
    }

    // Test sphere against all 6 planes
    bool contains_sphere(glm::vec3 center, float radius) const {
        for (const auto& p : planes)
            if (glm::dot(glm::vec3(p), center) + p.w < -radius)
                return false;
        return true;
    }
};
```

---

## 4.7 Normal Matrix

When an object is non-uniformly scaled, normals must be transformed with the **normal matrix**, not the model matrix:

```glsl
// Vertex shader
uniform mat4 model;
uniform mat3 normalMatrix;  // = transpose(inverse(mat3(model)))

vec3 worldNormal = normalize(normalMatrix * aNormal);
```

```cpp
// C++ side
glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
shader.setMat3("normalMatrix", normalMatrix);
```

> Computing `inverse` every frame is expensive — pass the normal matrix as a pre-computed uniform, not computed in the shader.

---

## Interview Questions

### Easy

**Q: What is the purpose of the model, view, and projection matrices?**

> **Model matrix**: transforms vertices from the object's local space to world space. It encodes position (translation), orientation (rotation), and scale of an object. **View matrix**: transforms from world space to camera space — effectively places the camera at the origin looking down -Z. Computed with `glm::lookAt(eye, target, up)`. **Projection matrix**: transforms from camera space to clip space, defining the view frustum. Perspective projection makes far objects smaller (realistic 3D). Orthographic projection has no foreshortening (CAD, 2D UI). In the vertex shader: `gl_Position = projection * view * model * vec4(vertex, 1.0)` — applied right-to-left.

**Q: Why is matrix multiplication order important in OpenGL?**

> OpenGL uses column-major matrices, and transformations apply right-to-left: `M = T * R * S` means the vertex is first scaled (S), then rotated (R), then translated (T). Reversing the order gives a completely different result — e.g., `T * S` (scale after translate) scales the object's displacement too. For MVP: `projection * view * model` — model applied first, then view (to camera space), then projection. This isn't a special OpenGL rule — it follows standard linear algebra: for matrix product AB, columns of AB = A applied to columns of B, so the rightmost matrix transforms the vector first.

**Q: What is `glm::lookAt` and what are its three parameters?**

> `glm::lookAt(eye, center, up)` builds a view matrix: `eye` = camera world-space position; `center` = the point the camera looks at; `up` = a vector pointing "up" (usually `{0,1,0}`). It computes three orthonormal basis vectors (right, up, forward) from these inputs and builds the matrix that transforms the world so the camera is at the origin looking down -Z. The resulting matrix is a combination of a rotation (to align axes) and a translation (to move the world so the camera is at origin). In camera space, X is right, Y is up, -Z is forward.

---

### Mid

**Q: What is Z-fighting and how do you prevent it?**

> Z-fighting is a rendering artifact where two coplanar (or nearly coplanar) surfaces flicker alternately, because floating-point depth values are too close to distinguish. The depth buffer has limited precision, and with perspective projection, depth resolution is non-linear — very compressed near the far plane. Solutions: (1) **Increase near plane**: the closer `near` is to zero, the less precision is available for the rest of the scene. `near=0.1` instead of `near=0.001` massively improves depth precision. (2) **Reduce far-to-near ratio**: aim for `far/near < 10,000`. (3) **Polygon offset**: `glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-1, -1)` shifts depth values to separate coplanar surfaces (used for decals). (4) **Reverse-Z**: use a reversed depth buffer (depth 1.0 = near, 0.0 = far) with a floating-point depth buffer — gives much better distribution of precision.

**Q: What is the difference between `gl_FragCoord` and texture coordinates?**

> `gl_FragCoord` is a built-in fragment shader input: `vec4(x, y, z, 1/w)` where `x,y` are the window-space pixel coordinates (e.g., 0–800, 0–600), `z` is the depth value (0–1, written to depth buffer), and `1/w` is the perspective correction factor. It's derived from `gl_Position` after rasterization. Texture coordinates (`TexCoord`) are user-defined `in` variables that the vertex shader passes to the fragment shader — they get linearly interpolated (perspective-corrected) across the triangle. `gl_FragCoord` is in pixel/screen space; texture UVs are in texture/UV space [0,1].

---

### Hard

**Q: Implement a view frustum culling system for a scene with 10,000 AABBs. What is the complexity and how can it be accelerated?**

> Per AABB vs 6 planes: test each corner against each plane, or use the "positive vertex" method (project AABB's closest corner in the plane normal direction): ```cpp bool aabb_in_frustum(const Frustum& f, glm::vec3 min, glm::vec3 max) { for (int i = 0; i < 6; i++) { glm::vec3 n(f.planes[i]); // Find the positive vertex (farthest in plane normal direction) glm::vec3 pv = { n.x >= 0 ? max.x : min.x, n.y >= 0 ? max.y : min.y, n.z >= 0 ? max.z : min.z }; if (glm::dot(n, pv) + f.planes[i].w < 0) return false; } return true; }``` Complexity: O(6 * 8) per AABB = constant, O(N) for N objects. For 10,000 objects: fast enough on CPU (10K × 6 dot products ≈ microseconds). Accelerations: (1) **Bounding Volume Hierarchy (BVH)**: test parent AABB first — if outside frustum, reject all children in O(log N). (2) **Temporal coherence**: objects rarely move between frustum inside/outside — cache results. (3) **GPU culling**: use a compute shader with `glDrawArraysIndirect` to cull on GPU without CPU readback.
