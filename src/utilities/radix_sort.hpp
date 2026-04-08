#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>

class RadixSort {
public:
    RadixSort() = default;
    ~RadixSort();

    void init(uint32_t maxElements);

    void sort(const glm::mat4& view,
              GLuint positionOpacitySSBO,
              uint32_t numElements);

    // Bind this to SSBO slot 1 for the vertex shader.
    GLuint sortedIndexBuffer() const;

    bool isInitialized() const { return mInitialized; }

private:
    // Ping-pong key/value buffers — A=ping, B=pong
    GLuint mKeyA      = 0;
    GLuint mKeyB      = 0;
    GLuint mValueA    = 0;
    GLuint mValueB    = 0;
    GLuint mHistogram = 0;
    GLuint mGlobalPfx = 0;   // 256 per-bucket totals for the global scan

    GLuint mKeyGenProgram = 0;
    GLuint mSortProgram   = 0;

    // keygen uniforms
    GLint mKG_View     = -1;
    GLint mKG_NumElem  = -1;

    // sort uniforms
    GLint mS_Pass      = -1;
    GLint mS_SubPass   = -1;
    GLint mS_NumElem   = -1;
    GLint mS_NumWG     = -1;

    uint32_t mMaxElements = 0;
    uint32_t mLastNumWG   = 0;   // cached so sortedIndexBuffer() knows which buffer has result
    bool     mInitialized = false;

    // Which buffer holds the final sorted values after the last sort() call.
    // true  → mValueB is the result  (odd number of passes written to B last)
    // false → mValueA is the result
    bool mResultInB = false;
};