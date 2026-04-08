#pragma once

#include "mesh.h"
#include "gaussian.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// std430 layout. 240 bytes total.
// We dropped color_pad — color is now derived from SH on the GPU.
// sh[12] holds 48 floats laid out as 16 RGB triplets:
//   sh_flat[0..2]   = DC  (band 0)
//   sh_flat[3..11]  = band 1 (3 coeffs * RGB)
//   sh_flat[12..26] = band 2 (5 coeffs * RGB)
//   sh_flat[27..47] = band 3 (7 coeffs * RGB)
// packed 4 floats per vec4.
struct GPUGaussian {
    glm::vec4 position_opacity; // xyz + activated opacity
    glm::vec4 cov3d_0;          // xx, xy, xz, yy
    glm::vec4 cov3d_1;          // yz, zz, pad, pad
    glm::vec4 sh[12];           // 48 SH floats (DC + 15 higher-order, RGB)
};

struct GaussianBuffers {
    GLuint vao = 0;
    GLuint quadVBO = 0;
    GLuint ssbo = 0;
    GLuint indexSSBO = 0;
    std::vector<GPUGaussian> gpuSplats;
    std::vector<int> sortedIndices;
    std::vector<std::pair<float,int>> sortKeys;
};

GaussianBuffers generateGaussianBuffer(const std::vector<GaussianData>& splats);

void sortGaussiansBackToFront(GaussianBuffers& buffers, const glm::mat4& view);

void updateGaussianSSBO(const GaussianBuffers& buffers);

static float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

static glm::mat3 quatToMat3(const glm::vec4& q) {
    float r = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;

    return glm::mat3(
        1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - r * z),       2.0f * (x * z + r * y),
        2.0f * (x * y + r * z),       1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z - r * x),
        2.0f * (x * z - r * y),       2.0f * (y * z + r * x),       1.0f - 2.0f * (x * x + y * y)
    );
}

static glm::mat3 computeCov3D(const glm::vec4& rotation, const glm::vec3& s) {
    glm::mat3 S(
        s.x, 0.0f, 0.0f,
        0.0f, s.y, 0.0f,
        0.0f, 0.0f, s.z
    );

    glm::mat3 R = quatToMat3(rotation);
    glm::mat3 M = S * R;
    return glm::transpose(M) * M;
}