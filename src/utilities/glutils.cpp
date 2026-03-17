#include <glad/glad.h>
#include <program.hpp>
#include "glutils.h"
#include <vector>
#include <glm/glm.hpp>


GaussianBuffers generateGaussianBuffer(const std::vector<GaussianData>& splats)
{
    GaussianBuffers buffers{};

    //VAO for quads
    glGenVertexArrays(1, &buffers.vao);
    glBindVertexArray(buffers.vao);

    const std::vector<glm::vec2> quadVertices = {
        {-1.0f, -1.0f},
        { 1.0f, -1.0f},
        { 1.0f,  1.0f},
        {-1.0f,  1.0f}
    };

    glGenBuffers(1, &buffers.quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.quadVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 quadVertices.size() * sizeof(glm::vec2),
                 quadVertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Converting splats to SSBO friendly format
    std::vector<GPUGaussian> gpuSplats;
    gpuSplats.reserve(splats.size());

    for (const auto& s : splats) {
        glm::vec3 color = glm::clamp(glm::vec3(0.5f) + 0.28209479f * s.f_dc,
                                    glm::vec3(0.0f), glm::vec3(1.0f));

        float opacity = sigmoid(s.opacity);
        glm::mat3 cov = computeCov3D(s.rotation, s.scale);

        gpuSplats.push_back({
            glm::vec4(s.position, opacity),
            glm::vec4(color, 0.0f),
            glm::vec4(cov[0][0], cov[0][1], cov[0][2], cov[1][1]),
            glm::vec4(cov[1][2], cov[2][2], 0.0f, 0.0f)
        });
    }

    // SSBO for gaussian data
    glGenBuffers(1, &buffers.ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuSplats.size() * sizeof(GPUGaussian),
                 gpuSplats.data(),
                 GL_STATIC_DRAW);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.ssbo);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    return buffers;
}

void sortGaussiansBackToFront(GaussianBuffers& buffers, const glm::mat4& view)
{
    // Extract just the Z row of the view matrix (row 2)
    // z = view[0][2]*x + view[1][2]*y + view[2][2]*z + view[3][2]
    const float vx = view[0][2], vy = view[1][2], vz = view[2][2], vw = view[3][2];

    int n = buffers.gpuSplats.size();
    std::vector<std::pair<float, int>> zIndex(n);

    for (int i = 0; i < n; i++) {
        const auto& p = buffers.gpuSplats[i].position_opacity;
        zIndex[i] = { vx*p.x + vy*p.y + vz*p.z + vw, i };
    }

    std::sort(zIndex.begin(), zIndex.end()); // sorts plain floats — very fast

    // Reorder (or just use indices directly in rendering)
    std::vector<GPUGaussian> sorted(n);
    for (int i = 0; i < n; i++)
        sorted[i] = buffers.gpuSplats[zIndex[i].second];
    buffers.gpuSplats = std::move(sorted);
}

void updateGaussianSSBO(const GaussianBuffers& buffers)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    0,
                    buffers.gpuSplats.size() * sizeof(GPUGaussian),
                    buffers.gpuSplats.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}