#version 430 core

layout(location = 0) in vec2 quadVertex;

uniform bool isPointCloud;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec2 tanHalfFov;
uniform float focalLength;

struct GPUGaussian {
    vec4 position_opacity; // xyz + activated opacity
    vec4 color_pad;        // rgb + pad
    vec4 cov3d_0;          // xx, xy, xz, yy
    vec4 cov3d_1;          // yz, zz, pad, pad
};

layout(std430, binding = 0) readonly buffer GaussianBuffer {
    GPUGaussian splats[];
};

out vec3 vColor;
out float vOpacity;
out vec3 vConic;
out vec2 vCoord;

out float debugRadius;

flat out int fragIsPointCloud;

mat3 unpackCov3D(GPUGaussian g)
{
    float xx = g.cov3d_0.x;
    float xy = g.cov3d_0.y;
    float xz = g.cov3d_0.z;
    float yy = g.cov3d_0.w;
    float yz = g.cov3d_1.x;
    float zz = g.cov3d_1.y;

    return mat3(
        xx, xy, xz,
        xy, yy, yz,
        xz, yz, zz
    );
}

vec3 projectCovarianceToScreen(vec3 meanWS, mat3 cov3D)
{
    vec3 t = (viewMatrix * vec4(meanWS, 1.0)).xyz;

    // OpenGL-style camera: points in front usually have negative z
    float depth = -t.z;

    if (depth <= 1e-6) {
        return vec3(-1.0, 0.0, -1.0);
    }

    float limX = 1.3 * tanHalfFov.x;
    float limY = 1.3 * tanHalfFov.y;

    float tx = t.x / depth;
    float ty = t.y / depth;

    t.x = clamp(tx, -limX, limX) * depth;
    t.y = clamp(ty, -limY, limY) * depth;

    mat3 J = mat3(
        focalLength / depth, 0.0,                   -(focalLength * t.x) / (depth * depth),
        0.0,                 focalLength / depth,   -(focalLength * t.y) / (depth * depth),
        0.0,                 0.0,                    0.0
    );

    mat3 W = transpose(mat3(viewMatrix));
    mat3 T = W * J;

    mat3 cov = transpose(T) * transpose(cov3D) * T;

    return vec3(cov[0][0], cov[0][1], cov[1][1]);
}

void main()
{
    GPUGaussian g = splats[gl_InstanceID];

    vec3 positionWS = g.position_opacity.xyz;
    float opacity = g.position_opacity.w;
    vec3 color = g.color_pad.xyz;

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
    vec3 cov2D = projectCovarianceToScreen(positionWS, cov3D);

    // slik originalen gjør det: først determinant uten low-pass
    float detCov = cov2D.x * cov2D.z - cov2D.y * cov2D.y;

    // // så low-pass
    cov2D.x += 0.3;
    cov2D.z += 0.3;

    float det = cov2D.x * cov2D.z - cov2D.y * cov2D.y;

    if (det <= 1e-8 || cov2D.x <= 0.0 || cov2D.z <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vColor = vec3(0.0);
        vOpacity = 0.0;
        vConic = vec3(1.0, 0.0, 1.0);
        vCoord = vec2(0.0);
        return;
    }

    // valgfri AA-skalering, tett på originalen
    float aaScale = sqrt(max(0.000025, detCov / det));
    float opacityAA = opacity * aaScale;

    float invDet = 1.0 / det;
    vConic = vec3(
        cov2D.z * invDet,
        -cov2D.y * invDet,
        cov2D.x * invDet
    );

    // bruk egenverdier til radius, ikke diagonalen direkte
    // float mid = 0.5 * (cov2D.x + cov2D.z);
    // float radiusTerm = max(0.1, mid * mid - det);
    // float lambda1 = mid + sqrt(radiusTerm);
    // float lambda2 = mid - sqrt(radiusTerm);

    // float radius = 3.0*sqrt(max(lambda1, lambda2));

    // float depth = positionCS.z;
    // debugRadius = abs(depth);

    // vec2 quadPixels = vec2(radius, radius);

        vec2 quadPixels = vec2(
        3.0 * sqrt(max(cov2D.x, 1e-8)),
        3.0 * sqrt(max(cov2D.z, 1e-8))
    );

    vec2 viewportScale = 2.0 * tanHalfFov * focalLength;
    vec2 quadNDC = quadPixels / viewportScale * 2.0;

    vec4 clip = projectionMatrix * positionCS;
    clip.xyz /= clip.w;
    clip.w = 1.0;
    clip.xy += quadVertex * quadNDC;

    gl_Position = clip;

    vColor = color;
    vOpacity = opacityAA;
    vCoord = quadVertex * quadPixels;
}