#pragma once

// System Headers
#include <glad/glad.h>

// Standard headers
#include <string>

// Constants
// #if 0
const int         windowWidth     = 1366;
const int         windowHeight    = 768;
const std::string windowTitle     = "Glowbox";
const GLint       windowResizable = GL_FALSE;
const int         windowSamples   = 4;
// #else
// const int         windowWidth     = 800;
// const int         windowHeight    = 600;
// const std::string windowTitle     = "Glowbox";
// const GLint       windowResizable = GL_FALSE;
// const int         windowSamples   = 0;
// #endif


struct CommandLineOptions {
    bool enableMusic    = false;
    bool enableAutoplay = false;
    bool record         = false; // enables orbit-recording mode (--record)
};