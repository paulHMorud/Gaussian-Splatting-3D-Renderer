#include "radix_sort.hpp"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>
#include <cmath>

#include <chrono>

// ---------------------------------------------------------------
// Constants matching the compute shader
// ---------------------------------------------------------------
static constexpr uint32_t BLOCK_SIZE   = 256;
static constexpr uint32_t RADIX        = 256;
static constexpr uint32_t NUM_PASSES   = 4;   // 4 × 8 bits = 32 bits

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
static std::string readFile(const char* path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[RadixSort] Cannot open shader: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileCS(const char* path)
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
        char log[2048];
        glGetShaderInfoLog(cs, sizeof(log), nullptr, log);
        std::cerr << "[RadixSort] Compile error in " << path << ":\n" << log << "\n";
        glDeleteShader(cs);
        return 0;
    }
    return cs;
}

static GLuint linkProgram(GLuint cs)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glDeleteShader(cs);   // shader object no longer needed after link

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[RadixSort] Link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static GLuint makeSSBO(size_t bytes, GLenum usage = GL_DYNAMIC_COPY)
{
    GLuint buf = 0;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, nullptr, usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buf;
}

// ---------------------------------------------------------------
// RadixSort::init
// ---------------------------------------------------------------
void RadixSort::init(uint32_t maxElements)
{
    mMaxElements = maxElements;

    // ---- compile shaders ----
    GLuint keygenCS = compileCS("../res/shaders/z_keygen.comp");
    GLuint sortCS   = compileCS("../res/shaders/radix_sort.comp");

    if (!keygenCS || !sortCS) {
        std::cerr << "[RadixSort] Shader compilation failed — GPU sort disabled.\n";
        return;
    }

    mKeyGenProgram = linkProgram(keygenCS);
    mSortProgram   = linkProgram(sortCS);

    if (!mKeyGenProgram || !mSortProgram) return;

    // ---- cache uniform locations ----
    mKeyGenLocView    = glGetUniformLocation(mKeyGenProgram, "uView");
    mKeyGenLocNumElem = glGetUniformLocation(mKeyGenProgram, "uNumElements");

    mLocPass    = glGetUniformLocation(mSortProgram, "uPass");
    mLocSubPass = glGetUniformLocation(mSortProgram, "uSubPass");
    mLocNumElem = glGetUniformLocation(mSortProgram, "uNumElements");

    // ---- allocate GPU buffers ----
    size_t keyBytes   = (size_t)maxElements * sizeof(uint32_t);
    size_t valBytes   = (size_t)maxElements * sizeof(int32_t);
    uint32_t maxWG    = (maxElements + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t histBytes  = (size_t)RADIX * maxWG * sizeof(uint32_t);

    mKeyA      = makeSSBO(keyBytes);
    mKeyB      = makeSSBO(keyBytes);
    mValueA    = makeSSBO(valBytes);
    mValueB    = makeSSBO(valBytes);
    mHistogram = makeSSBO(histBytes);

    mInitialized = true;
    std::cout << "[RadixSort] Initialized for " << maxElements << " elements.\n";
}

// ---------------------------------------------------------------
// RadixSort::~RadixSort
// ---------------------------------------------------------------
RadixSort::~RadixSort()
{
    GLuint bufs[] = { mKeyA, mKeyB, mValueA, mValueB, mHistogram };
    glDeleteBuffers(5, bufs);
    if (mSortProgram)   glDeleteProgram(mSortProgram);
    if (mKeyGenProgram) glDeleteProgram(mKeyGenProgram);
}

// ---------------------------------------------------------------
// Internal dispatch helpers
// ---------------------------------------------------------------
void RadixSort::dispatchCount(uint32_t pass, uint32_t numElements, uint32_t numWG)
{
    glUniform1ui(mLocPass,    pass);
    glUniform1ui(mLocSubPass, 0u);            // COUNT
    glUniform1ui(mLocNumElem, numElements);
    glDispatchCompute(numWG, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void RadixSort::dispatchPrefix(uint32_t pass, uint32_t numElements, uint32_t /*numWG*/)
{
    glUniform1ui(mLocPass,    pass);
    glUniform1ui(mLocSubPass, 1u);            // PREFIX
    glUniform1ui(mLocNumElem, numElements);
    glDispatchCompute(1, 1, 1);               // single workgroup of 256 threads
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void RadixSort::dispatchScatter(uint32_t pass, uint32_t numElements, uint32_t numWG)
{
    glUniform1ui(mLocPass,    pass);
    glUniform1ui(mLocSubPass, 2u);            // SCATTER
    glUniform1ui(mLocNumElem, numElements);
    glDispatchCompute(numWG, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// ---------------------------------------------------------------
// RadixSort::sort
// ---------------------------------------------------------------
void RadixSort::sort(const glm::mat4& view,
                     GLuint positionOpacitySSBO,
                     uint32_t numElements)
{
    if (!mInitialized || numElements == 0) return;
    assert(numElements <= mMaxElements);

    const uint32_t numWG = (numElements + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // ----------------------------------------------------------
    // Step 1: Key generation (GPU)
    // Writes view-space Z as sortable uint32 into mKeyA,
    // and identity indices (0..n-1) into mValueA.
    // ----------------------------------------------------------
    glUseProgram(mKeyGenProgram);

    glUniformMatrix4fv(mKeyGenLocView, 1, GL_FALSE, glm::value_ptr(view));
    glUniform1ui(mKeyGenLocNumElem, numElements);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionOpacitySSBO); // splat positions
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mKeyA);               // keys out
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mValueA);             // values out

    glDispatchCompute(numWG, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ----------------------------------------------------------
    // Step 2: 4-pass radix sort (ping-pong between A and B)
    // ----------------------------------------------------------
    glUseProgram(mSortProgram);

    // Ping-pong sources: even passes read A, write B; odd read B, write A.
    GLuint keys[2]   = { mKeyA,   mKeyB   };
    GLuint values[2] = { mValueA, mValueB };

    for (uint32_t pass = 0; pass < NUM_PASSES; pass++)
    {
        uint32_t src = pass & 1u;        // 0 or 1
        uint32_t dst = 1u - src;

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, keys[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, keys[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, values[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, values[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, mHistogram);

        dispatchCount  (pass, numElements, numWG);
        dispatchPrefix (pass, numElements, numWG);
        dispatchScatter(pass, numElements, numWG);
    }


//     glFinish(); // ensure previous work done
// auto t0 = std::chrono::high_resolution_clock::now();

// for (uint32_t pass = 0; pass < NUM_PASSES; pass++)
// {
//     uint32_t src = pass & 1u;
//     uint32_t dst = 1u - src;

//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, keys[src]);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, keys[dst]);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, values[src]);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, values[dst]);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, mHistogram);

//     glFinish();
//     auto ta = std::chrono::high_resolution_clock::now();
//     dispatchCount(pass, numElements, numWG);
//     glFinish();
//     auto tb = std::chrono::high_resolution_clock::now();
//     dispatchPrefix(pass, numElements, numWG);
//     glFinish();
//     auto tc = std::chrono::high_resolution_clock::now();
//     dispatchScatter(pass, numElements, numWG);
//     glFinish();
//     auto td = std::chrono::high_resolution_clock::now();

//     auto us = [](auto a, auto b){
//         return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();
//     };

//     std::cout << "Pass " << pass
//               << "  count="   << us(ta,tb) << "us"
//               << "  prefix="  << us(tb,tc) << "us"
//               << "  scatter=" << us(tc,td) << "us\n";
// }

// auto t1 = std::chrono::high_resolution_clock::now();
// std::cout << "Total sort: "
//           << std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()
//           << "ms\n";



    // After 4 passes (even count), result is in mValueA … but we always
    // want the final result in mValueB so sortedIndexBuffer() is stable.
    // NUM_PASSES=4 → last write was to keys[0]=mKeyA, values[0]=mValueA.
    // Swap so sortedIndexBuffer() returns the correct buffer.
    // (Alternatively, just check (NUM_PASSES & 1) at runtime.)
    if ((NUM_PASSES & 1u) == 0u) {
        // Final result landed in src=0 → mValueA. Swap so caller gets mValueB.
        std::swap(mValueA, mValueB);
    }
}