#include <glad/glad.h>
#include <program.hpp>
#include "glutils.h"
#include <vector>
#include <glm/glm.hpp>



GaussianBuffers generateGaussianBuffer(const std::vector<GaussianData>& splats)
{
    GaussianBuffers buffers{};
 
    glGenVertexArrays(1, &buffers.vao);
    glBindVertexArray(buffers.vao);
 
    // Two triangles (CCW), no fan — immune to face-culling winding issues
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


    for (int i = 0; i < 5; i++) {
        GaussianData s = splats[i];
        glm::mat3 cov = computeCov3D(s.rotation, s.scale);
        std::cout << cov[0][0] << " " << cov[1][0] << " " << cov[2][0] << "\n";
        std::cout << cov[0][1] << " " << cov[1][1] << " " << cov[2][1] << "\n";
        std::cout << cov[0][2] << " " << cov[1][2] << " " << cov[2][2] << "\n";

    }

 
    for (const auto& s : splats) {
        glm::vec3 color = glm::clamp(glm::vec3(0.5f) + 0.28209479f * s.f_dc,
                                     glm::vec3(0.0f), glm::vec3(1.0f));
 
        float opacity = sigmoid(s.opacity);
        glm::mat3 cov = computeCov3D(s.rotation, s.scale);
 
        // GLM is column-major: cov[col][row]
        // cov3d_0: xx, xy, xz, yy  →  [0][0], [1][0], [2][0], [1][1]
        // cov3d_1: yz, zz           →  [2][1], [2][2]
        buffers.gpuSplats.push_back({
            glm::vec4(s.position, opacity),
            glm::vec4(color, 0.0f),
            glm::vec4(cov[0][0], cov[1][0], cov[2][0], cov[1][1]),
            glm::vec4(cov[2][1], cov[2][2], 0.0f, 0.0f)
        });
    }

    std::cout << "sizeof(GPUGaussian): " << sizeof(GPUGaussian) << "\n";
    std::cout << "offset position_opacity: " << offsetof(GPUGaussian, position_opacity) << "\n";
    std::cout << "offset color_pad: " << offsetof(GPUGaussian, color_pad) << "\n";
    std::cout << "offset cov3d_0: " << offsetof(GPUGaussian, cov3d_0) << "\n";
    std::cout << "offset cov3d_1: " << offsetof(GPUGaussian, cov3d_1) << "\n";
 
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers.indexSSBO); // binding = 1
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
 
    return buffers;
}

// Sorting only indices instead of entire SSBO. As expected significantly sped up sorting
void sortGaussiansBackToFront(GaussianBuffers& buffers, const glm::mat4& view)
{
    const int n = (int)buffers.gpuSplats.size();

    // Extract the view-space Z row directly from the matrix —
    // avoids a full mat4*vec4 multiply per splat.
    // view-space Z = dot(row2, pos) = view[0][2]*x + view[1][2]*y + view[2][2]*z + view[3][2]
    const float vx = view[0][2], vy = view[1][2], vz = view[2][2], vw = view[3][2];

    for (int i = 0; i < n; i++) {
        const glm::vec4& p = buffers.gpuSplats[i].position_opacity;
        buffers.sortKeys[i] = { vx*p.x + vy*p.y + vz*p.z + vw, i };
    }

    std::sort(buffers.sortKeys.begin(), buffers.sortKeys.end());

    for (int i = 0; i < n; i++)
        buffers.sortedIndices[i] = buffers.sortKeys[i].second;

    // Upload only the index array (4 bytes/splat vs 48 bytes/splat)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.indexSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    n * sizeof(int),
                    buffers.sortedIndices.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// void sortGaussiansBackToFront(GaussianBuffers& buffers, const glm::mat4& view)
// {
//     int n = (int)buffers.gpuSplats.size();
//     std::vector<std::pair<float, int>> zIndex(n);

//     for (int i = 0; i < n; i++) {
//         const auto& p = buffers.gpuSplats[i].position_opacity;
//         glm::vec4 posVS = view * glm::vec4(p.x, p.y, p.z, 1.0f);
//         zIndex[i] = { posVS.z, i };
//     }

//     // In OpenGL view space, camera looks down -Z.
//     // Most negative Z = furthest away = draw first.
//     // Ascending sort puts most negative first = correct back-to-front.
//     std::sort(zIndex.begin(), zIndex.end());

//     std::vector<GPUGaussian> sorted(n);
//     for (int i = 0; i < n; i++)
//         sorted[i] = buffers.gpuSplats[zIndex[i].second];
//     buffers.gpuSplats = std::move(sorted);
// }

// void updateGaussianSSBO(const GaussianBuffers& buffers)
// {
//     glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers.ssbo);
//     glBufferSubData(GL_SHADER_STORAGE_BUFFER,
//                     0,
//                     buffers.gpuSplats.size() * sizeof(GPUGaussian),
//                     buffers.gpuSplats.data());
//     glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
// }