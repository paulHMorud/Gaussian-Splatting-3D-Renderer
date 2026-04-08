#include "radix_sort.hpp"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>

static constexpr uint32_t BLOCK_SIZE = 256;
static constexpr uint32_t RADIX      = 256;
static constexpr uint32_t NUM_PASSES = 4;

static constexpr uint32_t SP_COUNT        = 0;
static constexpr uint32_t SP_PFX_LOCAL    = 1;
static constexpr uint32_t SP_PFX_GLOBAL   = 2;
static constexpr uint32_t SP_APPLY_GLOBAL = 3;
static constexpr uint32_t SP_SCATTER      = 4;

static std::string readFile(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[RadixSort] Cannot open: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileAndLink(const char* path)
{
    std::string src = readFile(path);
    if (src.empty()) return 0;

    const char* csrc = src.c_str();
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &csrc, nullptr);
    glCompileShader(cs);

    GLint ok = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(cs, sizeof(log), nullptr, log);
        std::cerr << "[RadixSort] Compile error in " << path << ":\n" << log << "\n";
        glDeleteShader(cs); return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glDeleteShader(cs);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[RadixSort] Link error:\n" << log << "\n";
        glDeleteProgram(prog); return 0;
    }
    return prog;
}

static GLuint makeSSBO(size_t bytes)
{
    GLuint buf = 0;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buf;
}

void RadixSort::init(uint32_t maxElements)
{
    mMaxElements = maxElements;

    mKeyGenProgram = compileAndLink("../res/shaders/z_keygen.comp");
    mSortProgram   = compileAndLink("../res/shaders/radix_sort.comp");
    if (!mKeyGenProgram || !mSortProgram) return;

    mKG_View       = glGetUniformLocation(mKeyGenProgram, "uView");
    mKG_Projection = glGetUniformLocation(mKeyGenProgram, "uProjection");
    mKG_NumElem    = glGetUniformLocation(mKeyGenProgram, "uNumElements");
    mKG_CullMargin = glGetUniformLocation(mKeyGenProgram, "uCullMargin");

    mS_Pass    = glGetUniformLocation(mSortProgram, "uPass");
    mS_SubPass = glGetUniformLocation(mSortProgram, "uSubPass");
    mS_NumElem = glGetUniformLocation(mSortProgram, "uNumElements");
    mS_NumWG   = glGetUniformLocation(mSortProgram, "uNumWorkGroups");

    const uint32_t maxWG = (maxElements + BLOCK_SIZE - 1) / BLOCK_SIZE;

    mKeyA         = makeSSBO((size_t)maxElements * sizeof(uint32_t));
    mKeyB         = makeSSBO((size_t)maxElements * sizeof(uint32_t));
    mValueA       = makeSSBO((size_t)maxElements * sizeof(int32_t));
    mValueB       = makeSSBO((size_t)maxElements * sizeof(int32_t));
    mHistogram    = makeSSBO((size_t)RADIX * maxWG * sizeof(uint32_t));
    mGlobalPfx    = makeSSBO((size_t)RADIX * sizeof(uint32_t));
    mVisibleCount = makeSSBO(sizeof(uint32_t));

    mInitialized = true;
    std::cout << "[RadixSort] Initialized for " << maxElements
              << " elements (" << maxWG << " max workgroups).\n";
}

RadixSort::~RadixSort()
{
    GLuint bufs[] = { mKeyA, mKeyB, mValueA, mValueB,
                      mHistogram, mGlobalPfx, mVisibleCount };
    glDeleteBuffers(7, bufs);
    if (mSortProgram)   glDeleteProgram(mSortProgram);
    if (mKeyGenProgram) glDeleteProgram(mKeyGenProgram);
}

GLuint RadixSort::sortedIndexBuffer() const
{
    return mResultInB ? mValueB : mValueA;
}

uint32_t RadixSort::sort(const glm::mat4& view,
                         const glm::mat4& projection,
                         GLuint positionOpacitySSBO,
                         uint32_t numElements,
                         float cullMargin)
{
    if (!mInitialized || numElements == 0) return 0;
    assert(numElements <= mMaxElements);

    // ----------------------------------------------------------
    // Step 1: Reset the visible counter to 0
    // ----------------------------------------------------------
    const uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVisibleCount);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ----------------------------------------------------------
    // Step 2: Keygen + frustum cull → compact into key/value A
    // ----------------------------------------------------------
    const uint32_t numWG = (numElements + BLOCK_SIZE - 1) / BLOCK_SIZE;

    glUseProgram(mKeyGenProgram);
    glUniformMatrix4fv(mKG_View,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(mKG_Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1ui(mKG_NumElem,    numElements);
    glUniform1f (mKG_CullMargin, cullMargin);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionOpacitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mKeyA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mValueA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, mVisibleCount);

    glDispatchCompute(numWG, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ----------------------------------------------------------
    // Step 3: Read back visible count (tiny 4-byte readback)
    // ----------------------------------------------------------
    uint32_t visibleCount = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVisibleCount);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &visibleCount);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (visibleCount == 0) return 0;

    // ----------------------------------------------------------
    // Step 4: Radix sort over only the visible splats
    // ----------------------------------------------------------
    const uint32_t sortWG = (visibleCount + BLOCK_SIZE - 1) / BLOCK_SIZE;

    glUseProgram(mSortProgram);
    glUniform1ui(mS_NumElem, visibleCount);
    glUniform1ui(mS_NumWG,   sortWG);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, mGlobalPfx);

    GLuint keys[2]   = { mKeyA,   mKeyB   };
    GLuint values[2] = { mValueA, mValueB };

    for (uint32_t pass = 0; pass < NUM_PASSES; pass++)
    {
        uint32_t src = pass & 1u;
        uint32_t dst = 1u - src;

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, keys[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, keys[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, values[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, values[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, mHistogram);

        glUniform1ui(mS_Pass, pass);

        glUniform1ui(mS_SubPass, SP_COUNT);
        glDispatchCompute(sortWG, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUniform1ui(mS_SubPass, SP_PFX_LOCAL);
        glDispatchCompute(RADIX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUniform1ui(mS_SubPass, SP_PFX_GLOBAL);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUniform1ui(mS_SubPass, SP_APPLY_GLOBAL);
        glDispatchCompute(RADIX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glUniform1ui(mS_SubPass, SP_SCATTER);
        glDispatchCompute(sortWG, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    mResultInB = (NUM_PASSES & 1u) != 0u;
    return visibleCount;
}