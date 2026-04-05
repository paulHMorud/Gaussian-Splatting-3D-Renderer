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
        vec3(xx, xy, xz),
        vec3(xy, yy, yz),
        vec3(xz, yz, zz)
    );
}

// Returns 2D covariance
// Changed this func to an almost copy of the function cov2d() from https://github.com/LytixDev/3d-gaussian-splatting-renderer/blob/main/res/shaders/gaussian.vert 
// This in the debugging stage as I cant find out why my program is not working. 
vec3 projectCovarianceToScreen(vec4 meanCS, mat3 cov3D)
{
    // vec4 t4 = viewMatrix * vec4(meanCS, 1.0);
    // vec3 t = t4.xyz;
    vec4 t = meanCS; // Test

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

    // Applying low pass filter
    cov[0][0] += 0.3f;
	cov[1][1] += 0.3f;

    return vec3(cov[0][0], cov[0][1], cov[1][1]);
}


void main()
{
    GPUGaussian g = splats[gl_InstanceID];

    vec3 positionWS = g.position_opacity.xyz; // World space
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

    vec4 positionCS = viewMatrix * vec4(positionWS, 1.0); //Camers space
    mat3 cov3D = unpackCov3D(g);

    if (positionCS.z > -0.1) {
        gl_Position = vec4(2.0);
        vOpacity = 0.0;
        return;
    }

    vec3 cov2D = projectCovarianceToScreen(positionCS, cov3D);

    // Apply low-pass filter exactly once.
    // cov2D.x += 0.3;
    // cov2D.z += 0.3;


    float det = cov2D.x * cov2D.z - cov2D.y * cov2D.y;

    // if (det <= 1e-8 || cov2D.x <= 0.0 || cov2D.z <= 0.0) {
    //     gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    //     vColor = vec3(0.0);
    //     vOpacity = 0.0;
    //     vConic = vec3(1.0, 0.0, 1.0);
    //     vCoord = vec2(0.0);
    //     return;
    // }

    float invDet = 1.0 / det;
    vConic = vec3(
        cov2D.z * invDet,
        -cov2D.y * invDet,
        cov2D.x * invDet
    );


    float mid = 0.5 * (cov2D.x + cov2D.z);
    float disc = max(1e-8, mid * mid - det);
    float lambda1 = mid + sqrt(disc);
    float lambda2 = mid - sqrt(disc);

    // vec2 quadPixels = 3.0 * vec2(sqrt(max(lambda1, 1e-8)),
    //                              sqrt(max(lambda2, 1e-8)));

    vec2 quadPixels = vec2(3.0 * sqrt(cov2D.x), 3.0 * sqrt(cov2D.z));

    vec2 viewportScale = 2.0 * tanHalfFov * focalLength;
    vec2 quadNDC = quadPixels / viewportScale * 2.0;

    // vec4 clip = projectionMatrix * positionCS;
    // clip.xy += quadVertex * quadNDC * clip.w;

    // gl_Position = clip;

    vec4 clip = projectionMatrix * positionCS;
    clip.xyz /= clip.w;
    clip.w = 1.0;
    clip.xy += quadVertex * quadNDC;
    gl_Position = clip;

    vColor = color;
    vOpacity = opacity;
    vCoord = quadVertex * quadPixels;
}
