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

// Sub-pass IDs matching the shader
static constexpr uint32_t SP_COUNT        = 0;
static constexpr uint32_t SP_PFX_LOCAL    = 1;
static constexpr uint32_t SP_PFX_GLOBAL   = 2;
static constexpr uint32_t SP_APPLY_GLOBAL = 3;
static constexpr uint32_t SP_SCATTER      = 4;

// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
void RadixSort::init(uint32_t maxElements)
{
    mMaxElements = maxElements;

    mKeyGenProgram = compileAndLink("../res/shaders/z_keygen.comp");
    mSortProgram   = compileAndLink("../res/shaders/radix_sort.comp");
    if (!mKeyGenProgram || !mSortProgram) return;

    mKG_View    = glGetUniformLocation(mKeyGenProgram, "uView");
    mKG_NumElem = glGetUniformLocation(mKeyGenProgram, "uNumElements");

    mS_Pass    = glGetUniformLocation(mSortProgram, "uPass");
    mS_SubPass = glGetUniformLocation(mSortProgram, "uSubPass");
    mS_NumElem = glGetUniformLocation(mSortProgram, "uNumElements");
    mS_NumWG   = glGetUniformLocation(mSortProgram, "uNumWorkGroups");

    const uint32_t maxWG = (maxElements + BLOCK_SIZE - 1) / BLOCK_SIZE;

    mKeyA      = makeSSBO((size_t)maxElements * sizeof(uint32_t));
    mKeyB      = makeSSBO((size_t)maxElements * sizeof(uint32_t));
    mValueA    = makeSSBO((size_t)maxElements * sizeof(int32_t));
    mValueB    = makeSSBO((size_t)maxElements * sizeof(int32_t));
    mHistogram = makeSSBO((size_t)RADIX * maxWG * sizeof(uint32_t));
    mGlobalPfx = makeSSBO((size_t)RADIX * sizeof(uint32_t));

    mInitialized = true;
    std::cout << "[RadixSort] Initialized for " << maxElements
              << " elements (" << maxWG << " workgroups).\n";
}

// ---------------------------------------------------------------
RadixSort::~RadixSort()
{
    GLuint bufs[] = { mKeyA, mKeyB, mValueA, mValueB, mHistogram, mGlobalPfx };
    glDeleteBuffers(6, bufs);
    if (mSortProgram)   glDeleteProgram(mSortProgram);
    if (mKeyGenProgram) glDeleteProgram(mKeyGenProgram);
}

// ---------------------------------------------------------------
GLuint RadixSort::sortedIndexBuffer() const
{
    // After NUM_PASSES=4 passes the ping-pong lands as follows:
    // pass 0: src=A dst=B
    // pass 1: src=B dst=A
    // pass 2: src=A dst=B
    // pass 3: src=B dst=A  ← final result in A (mValueA)
    // So with 4 passes, result is always in mValueA.
    return mResultInB ? mValueB : mValueA;
}

// ---------------------------------------------------------------
void RadixSort::sort(const glm::mat4& view,
                     GLuint positionOpacitySSBO,
                     uint32_t numElements)
{
    if (!mInitialized || numElements == 0) return;
    assert(numElements <= mMaxElements);

    const uint32_t numWG = (numElements + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // ----------------------------------------------------------
    // Step 1: Key generation
    // ----------------------------------------------------------
    glUseProgram(mKeyGenProgram);
    glUniformMatrix4fv(mKG_View,    1, GL_FALSE, glm::value_ptr(view));
    glUniform1ui(mKG_NumElem, numElements);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionOpacitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mKeyA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mValueA);

    glDispatchCompute(numWG, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ----------------------------------------------------------
    // Step 2: 4-pass radix sort
    // ----------------------------------------------------------
    glUseProgram(mSortProgram);
    glUniform1ui(mS_NumElem, numElements);
    glUniform1ui(mS_NumWG,   numWG);

    // Always bind global prefix buffer
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

        // COUNT
        glUniform1ui(mS_SubPass, SP_COUNT);
        glDispatchCompute(numWG, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // PREFIX_LOCAL — one workgroup per bucket
        glUniform1ui(mS_SubPass, SP_PFX_LOCAL);
        glDispatchCompute(RADIX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // PREFIX_GLOBAL — single workgroup scans 256 bucket totals
        glUniform1ui(mS_SubPass, SP_PFX_GLOBAL);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // APPLY_GLOBAL — add global offsets back into histogram rows
        glUniform1ui(mS_SubPass, SP_APPLY_GLOBAL);
        glDispatchCompute(RADIX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // SCATTER
        glUniform1ui(mS_SubPass, SP_SCATTER);
        glDispatchCompute(numWG, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // With NUM_PASSES=4 (even), last scatter wrote to dst=(4-1 & 1)^1 = 0 = A
    // So result is in mValueA → mResultInB = false
    mResultInB = (NUM_PASSES & 1u) != 0u;
}