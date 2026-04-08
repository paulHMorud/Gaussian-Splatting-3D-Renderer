#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>

// ---------------------------------------------------------------
// RadixSort
//
// GPU radix sort for 3DGS splat depth ordering.
// Sorts a (uint32 key, int value) buffer entirely on the GPU.
//
// Keys are view-space Z values encoded as uint32 so that the
// natural unsigned integer sort order matches float sort order
// (the floatToSortKey encoding in the shader handles negatives).
//
// Usage:
//   RadixSort sorter;
//   sorter.init(numSplats);
//
//   // Every frame (or every N frames):
//   sorter.sort(view, positionBuffer, numSplats);
//
//   // Bind the sorted index buffer to binding point 1 for the
//   // vertex shader (same slot your current indexSSBO uses):
//   glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sorter.sortedIndexBuffer());
// ---------------------------------------------------------------

class RadixSort {
public:
    RadixSort() = default;
    ~RadixSort();

    // Allocate GPU buffers for up to `maxElements` splats.
    // Call once after the OpenGL context is ready.
    void init(uint32_t maxElements);

    // Upload new Z-keys derived from `view` × positions, then sort.
    // `positionOpacitySSBO` is binding-0 (your existing splat SSBO).
    // After this call, sortedIndexBuffer() holds the sorted indices.
    void sort(const glm::mat4& view,
              GLuint positionOpacitySSBO,
              uint32_t numElements);

    // The sorted index buffer — bind this to slot 1 for the vertex shader.
    GLuint sortedIndexBuffer() const { return mValueB; }

    bool isInitialized() const { return mInitialized; }

private:
    // --- compile helpers ---
    GLuint compileComputeShader(const char* path);
    GLuint linkComputeProgram(GLuint cs);

    // --- sub-dispatch helpers ---
    void dispatchCount  (uint32_t pass, uint32_t numElements, uint32_t numWG);
    void dispatchPrefix (uint32_t pass, uint32_t numElements, uint32_t numWG);
    void dispatchScatter(uint32_t pass, uint32_t numElements, uint32_t numWG);

    // --- GPU buffers ---
    GLuint mKeyA   = 0;   // ping  — uint keys
    GLuint mKeyB   = 0;   // pong
    GLuint mValueA = 0;   // ping  — int indices
    GLuint mValueB = 0;   // pong  (this is sortedIndexBuffer after even #passes)
    GLuint mHistogram = 0;

    // --- shader for building Z-keys from the splat SSBO ---
    GLuint mKeyGenProgram   = 0;

    // --- radix sort compute program ---
    GLuint mSortProgram = 0;

    // --- uniform locations ---
    GLuint mLocPass    = 0;
    GLuint mLocSubPass = 0;
    GLuint mLocNumElem = 0;

    GLuint mKeyGenLocView    = 0;
    GLuint mKeyGenLocNumElem = 0;

    uint32_t mMaxElements = 0;
    bool     mInitialized = false;
};