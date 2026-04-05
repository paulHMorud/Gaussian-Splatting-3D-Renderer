#pragma once

#include "mesh.h"
#include "gaussian.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GPUGaussian {
    glm::vec4 position_opacity; // xyz + activated opacity
    glm::vec4 color_pad;        // rgb + pad
    glm::vec4 cov3d_0;          // xx, xy, xz, yy
    glm::vec4 cov3d_1;          // yz, zz, pad, pad
};

struct GaussianBuffers {
    GLuint vao = 0;
    GLuint quadVBO = 0;
    GLuint ssbo = 0;
    std::vector<GPUGaussian> gpuSplats;
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