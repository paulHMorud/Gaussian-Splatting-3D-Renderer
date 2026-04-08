#include <glad/glad.h>
#include <program.hpp>
#include "glutils.h"
#include <vector>
#include <numeric>
#include <iostream>
#include <glm/glm.hpp>


GaussianBuffers generateGaussianBuffer(const std::vector<GaussianData>& splats)
{
    GaussianBuffers buffers{};

    glGenVertexArrays(1, &buffers.vao);
    glBindVertexArray(buffers.vao);

    const std::vector<glm::vec2> quadVertices = {
        {-1.0f, -1.0f},
        { 1.0f, -1.0f},
        { 1.0f,  1.0f},
        {-1.0f, -1.0f},
        { 1.0f,  1.0f},
        {-1.0f,  1.0f},
    };

    glGenBuffers(1, &buffers.quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.quadVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 quadVertices.size() * sizeof(glm::vec2),
                 quadVertices.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    buffers.gpuSplats.reserve(splats.size());

    for (const auto& s : splats) {
        float opacity = sigmoid(s.opacity);
        glm::mat3 cov = computeCov3D(s.rotation, s.scale);

        GPUGaussian gpu{};
        gpu.position_opacity = glm::vec4(s.position, opacity);
        gpu.cov3d_0          = glm::vec4(cov[0][0], cov[1][0], cov[2][0], cov[1][1]);
        gpu.cov3d_1          = glm::vec4(cov[2][1], cov[2][2], 0.0f, 0.0f);

        // ----- Pack SH coefficients ------------------------------------------
        // PLY layout (INRIA format) for f_rest is CHANNEL-major:
        //   f_rest[ 0..14] = R coeffs for bands 1..3 (15 values)
        //   f_rest[15..29] = G coeffs
        //   f_rest[30..44] = B coeffs
        // We repack into a flat 48-float array as 16 RGB triplets:
        //   sh_flat[3*c+0..2] = (R,G,B) for coefficient c, c in [0..15]
        // Coefficient 0 is the DC term from f_dc.
        float sh_flat[48];

        sh_flat[0] = s.f_dc.x;
        sh_flat[1] = s.f_dc.y;
        sh_flat[2] = s.f_dc.z;

        for (int c = 1; c < 16; ++c) {
            int rest = c - 1; // 0..14
            sh_flat[3 * c + 0] = s.f_rest[rest +  0]; // R
            sh_flat[3 * c + 1] = s.f_rest[rest + 15]; // G
            sh_flat[3 * c + 2] = s.f_rest[rest + 30]; // B
        }

        // Pack 48 floats into 12 vec4s (std430 vec4 stride)
        for (int i = 0; i < 12; ++i) {
            gpu.sh[i] = glm::vec4(
                sh_flat[4 * i + 0],
                sh_flat[4 * i + 1],
                sh_flat[4 * i + 2],
                sh_flat[4 * i + 3]
            );
        }

        buffers.gpuSplats.push_back(gpu);
    }

    std::cout << "sizeof(GPUGaussian): " << sizeof(GPUGaussian) << "\n";
    std::cout << "offset position_opacity: " << offsetof(GPUGaussian, position_opacity) << "\n";
    std::cout << "offset cov3d_0:          " << offsetof(GPUGaussian, cov3d_0) << "\n";
    std::cout << "offset cov3d_1:          " << offsetof(GPUGaussian, cov3d_1) << "\n";
    std::cout << "offset sh:               " << offsetof(GPUGaussian, sh) << "\n";

    glGenBuffers(1, &buffers.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 buffers.gpuSplats.size() * sizeof(GPUGaussian),
                 buffers.gpuSplats.data(),
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    int n = (int)buffers.gpuSplats.size();
    buffers.sortedIndices.resize(n);
    buffers.sortKeys.resize(n);
    std::iota(buffers.sortedIndices.begin(), buffers.sortedIndices.end(), 0);

    glGenBuffers(1, &buffers.indexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.indexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 n * sizeof(int),
                 buffers.sortedIndices.data(),
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers.indexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return buffers;
}

void sortGaussiansBackToFront(GaussianBuffers& buffers, const glm::mat4& view)
{
    const int n = (int)buffers.gpuSplats.size();
    const float vx = view[0][2], vy = view[1][2], vz = view[2][2], vw = view[3][2];

    for (int i = 0; i < n; i++) {
        const glm::vec4& p = buffers.gpuSplats[i].position_opacity;
        buffers.sortKeys[i] = { vx*p.x + vy*p.y + vz*p.z + vw, i };
    }

    std::sort(buffers.sortKeys.begin(), buffers.sortKeys.end());

    for (int i = 0; i < n; i++)
        buffers.sortedIndices[i] = buffers.sortKeys[i].second;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.indexSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    n * sizeof(int),
                    buffers.sortedIndices.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}