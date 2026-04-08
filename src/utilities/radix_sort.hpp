#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>

class RadixSort {
public:
    RadixSort() = default;
    ~RadixSort();

    void init(uint32_t maxElements);

    // Returns the number of visible splats after culling.
    // Pass this to glDrawArraysInstanced instead of total splat count.
    uint32_t sort(const glm::mat4& view,
                  const glm::mat4& projection,
                  GLuint positionOpacitySSBO,
                  uint32_t numElements,
                  float cullMargin = 1.2f);

    // Bind to SSBO slot 1 for the vertex shader
    GLuint sortedIndexBuffer() const;

    bool isInitialized() const { return mInitialized; }

private:
    GLuint mKeyA         = 0;
    GLuint mKeyB         = 0;
    GLuint mValueA       = 0;
    GLuint mValueB       = 0;
    GLuint mHistogram    = 0;
    GLuint mGlobalPfx    = 0;
    GLuint mVisibleCount = 0;   // atomic counter SSBO (1 uint)

    GLuint mKeyGenProgram = 0;
    GLuint mSortProgram   = 0;

    // keygen uniforms
    GLint mKG_View       = -1;
    GLint mKG_Projection = -1;
    GLint mKG_NumElem    = -1;
    GLint mKG_CullMargin = -1;

    // sort uniforms
    GLint mS_Pass    = -1;
    GLint mS_SubPass = -1;
    GLint mS_NumElem = -1;
    GLint mS_NumWG   = -1;

    uint32_t mMaxElements  = 0;
    bool     mResultInB    = false;
    bool     mInitialized  = false;
};