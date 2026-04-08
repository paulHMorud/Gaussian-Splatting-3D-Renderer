#version 430 core
// 3D Gaussian Splatting vertex shader with view-dependent color via spherical harmonics.

layout(location = 0) in vec2 quadVertex;

uniform bool  isPointCloud;
uniform mat4  viewMatrix;
uniform mat4  projectionMatrix;
uniform vec2  tanHalfFov;
uniform float focalLength;

// SH controls
uniform int  uShDegree;   // 0 = DC only (flat color), 1/2/3 = use bands up to that degree
uniform vec3 uCameraPos;  // camera position in the renderer's (flipped) world space


struct GPUGaussian {
    vec4 position_opacity;
    vec4 cov3d_0;
    vec4 cov3d_1;
    vec4 sh[12]; // 48 floats: 16 RGB triplets, coefficient-major
};

layout(std430, binding = 0) readonly buffer GaussianBuffer {
    GPUGaussian splats[];
};
layout(std430, binding = 1) readonly buffer IndexBuffer {
    int sortedIndices[];
};

out vec3  vColor;
out float vOpacity;
out vec3  vConic;
out vec2  vCoord;
flat out int fragIsPointCloud;


// ---------- SH helpers ------------------------------------------------------
const float SH_C0 = 0.28209479177387814;
const float SH_C1 = 0.4886025119029199;
const float SH_C2_0 =  1.0925484305920792;
const float SH_C2_1 = -1.0925484305920792;
const float SH_C2_2 =  0.31539156525252005;
const float SH_C2_3 = -1.0925484305920792;
const float SH_C2_4 =  0.5462742152960396;
const float SH_C3_0 = -0.5900435899266435;
const float SH_C3_1 =  2.890611442640554;
const float SH_C3_2 = -0.4570457994644658;
const float SH_C3_3 =  0.3731763325901154;
const float SH_C3_4 = -0.4570457994644658;
const float SH_C3_5 =  1.445305721320277;
const float SH_C3_6 = -0.5900435899266435;

// Returns RGB triplet for SH coefficient index c (0..15)
vec3 shCoeff(GPUGaussian g, int c)
{
    int base = 3 * c;          // float index in flat array
    int v0 = base >> 2;        // which vec4
    int o0 = base & 3;         // offset within that vec4
    // Read three consecutive floats; they may straddle a vec4 boundary.
    float f0 = g.sh[v0][o0];
    int idx1 = base + 1;
    int idx2 = base + 2;
    float f1 = g.sh[idx1 >> 2][idx1 & 3];
    float f2 = g.sh[idx2 >> 2][idx2 & 3];
    return vec3(f0, f1, f2);
}

vec3 evalSH(GPUGaussian g, vec3 dir, int degree)
{
    vec3 result = SH_C0 * shCoeff(g, 0);

    if (degree >= 1) {
        float x = dir.x, y = dir.y, z = dir.z;
        result += -SH_C1 * y * shCoeff(g, 1)
                +  SH_C1 * z * shCoeff(g, 2)
                + -SH_C1 * x * shCoeff(g, 3);

        if (degree >= 2) {
            float xx = x*x, yy = y*y, zz = z*z;
            float xy = x*y, yz = y*z, xz = x*z;
            result += SH_C2_0 * xy            * shCoeff(g, 4)
                   +  SH_C2_1 * yz            * shCoeff(g, 5)
                   +  SH_C2_2 * (2.0*zz - xx - yy) * shCoeff(g, 6)
                   +  SH_C2_3 * xz            * shCoeff(g, 7)
                   +  SH_C2_4 * (xx - yy)     * shCoeff(g, 8);

            if (degree >= 3) {
                result += SH_C3_0 * y * (3.0*xx - yy)        * shCoeff(g,  9)
                       +  SH_C3_1 * xy * z                    * shCoeff(g, 10)
                       +  SH_C3_2 * y * (4.0*zz - xx - yy)    * shCoeff(g, 11)
                       +  SH_C3_3 * z * (2.0*zz - 3.0*xx - 3.0*yy) * shCoeff(g, 12)
                       +  SH_C3_4 * x * (4.0*zz - xx - yy)    * shCoeff(g, 13)
                       +  SH_C3_5 * z * (xx - yy)             * shCoeff(g, 14)
                       +  SH_C3_6 * x * (xx - 3.0*yy)         * shCoeff(g, 15);
            }
        }
    }

    // Convention from the original 3DGS code: SH represents (color - 0.5)
    return max(result + 0.5, vec3(0.0));
}
// ---------------------------------------------------------------------------


mat3 unpackCov3D(GPUGaussian g)
{
    float xx = g.cov3d_0.x;
    float xy = g.cov3d_0.y;
    float xz = g.cov3d_0.z;
    float yy = g.cov3d_0.w;
    float yz = g.cov3d_1.x;
    float zz = g.cov3d_1.y;

    return mat3(
        vec3(xx, xy, xz),
        vec3(xy, yy, yz),
        vec3(xz, yz, zz)
    );
}

vec3 projectCovarianceToScreen(vec4 meanCS, mat3 cov3D)
{
    vec4 t = meanCS;

    float limX = 1.3 * tanHalfFov.x;
    float limY = 1.3 * tanHalfFov.y;

    float txtz = t.x / t.z;
    float tytz = t.y / t.z;

    t.x = clamp(txtz, -limX, limX) * t.z;
    t.y = clamp(tytz, -limY, limY) * t.z;

    mat3 J = mat3(
        focalLength / t.z, 0.0, -(focalLength * t.x) / (t.z * t.z),
        0.0, focalLength / t.z, -(focalLength * t.y) / (t.z * t.z),
        0.0, 0.0, 0.0
    );

    mat3 W = transpose(mat3(viewMatrix));
    mat3 T = W * J;

    mat3 cov = transpose(T) * transpose(cov3D) * T;

    cov[0][0] += 0.3;
    cov[1][1] += 0.3;

    return vec3(cov[0][0], cov[0][1], cov[1][1]);
}


void main()
{
    GPUGaussian g = splats[sortedIndices[gl_InstanceID]];

    vec3 positionWS = g.position_opacity.xyz;
    float opacity   = g.position_opacity.w;

    // ---- Compute view-dependent color via SH -----------------------------
    // The PLY loader negates x and y to flip handedness, so the renderer's
    // world space is mirrored relative to the space the SH was trained in.
    // To evaluate SH correctly we negate x and y of the direction so it lives
    // in the original training space.
    // (If your colors look wrong front-to-back, try removing the negation.)
    vec3 dirRender = normalize(positionWS - uCameraPos);
    vec3 dirSH = vec3(-dirRender.x, -dirRender.y, dirRender.z);

    vec3 color = evalSH(g, dirSH, clamp(uShDegree, 0, 3));
    // ----------------------------------------------------------------------

    fragIsPointCloud = isPointCloud ? 1 : 0;

    if (isPointCloud) {
        vec4 positionCS = viewMatrix * vec4(positionWS, 1.0);
        gl_Position = projectionMatrix * positionCS;
        gl_PointSize = 3.0;

        vColor = color;
        vOpacity = opacity;
        vConic = vec3(1.0, 0.0, 1.0);
        vCoord = vec2(0.0);
        return;
    }

    vec4 positionCS = viewMatrix * vec4(positionWS, 1.0);
    mat3 cov3D = unpackCov3D(g);

    if (positionCS.z > -0.1) {
        gl_Position = vec4(2.0);
        vOpacity = 0.0;
        return;
    }

    vec3 cov2D = projectCovarianceToScreen(positionCS, cov3D);

    float det = cov2D.x * cov2D.z - cov2D.y * cov2D.y;

    if (det <= 1e-8 || cov2D.x <= 0.0 || cov2D.z <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vColor = vec3(0.0);
        vOpacity = 0.0;
        vConic = vec3(1.0, 0.0, 1.0);
        vCoord = vec2(0.0);
        return;
    }

    float invDet = 1.0 / det;
    vConic = vec3(
        cov2D.z * invDet,
        -cov2D.y * invDet,
        cov2D.x * invDet
    );

    vec2 quadPixels = vec2(3.0 * sqrt(cov2D.x), 3.0 * sqrt(cov2D.z));

    vec2 viewportScale = 2.0 * tanHalfFov * focalLength;
    vec2 quadNDC = quadPixels / viewportScale * 2.0;

    vec4 clip = projectionMatrix * positionCS;
    clip.xyz /= clip.w;
    clip.w = 1.0;
    clip.xy += quadVertex * quadNDC;
    gl_Position = clip;

    vColor = color;
    vOpacity = opacity;
    vCoord = quadVertex * quadPixels;
}