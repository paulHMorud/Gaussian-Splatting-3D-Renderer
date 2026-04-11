#pragma once
#include <GLFW/glfw3.h>
#include <utilities/window.hpp>
#include "utilities/camera.hpp"


void runProgram(GLFWwindow* window, CommandLineOptions options);
void initGame(GLFWwindow* window, CommandLineOptions options);
void renderFrame(GLFWwindow* window);
void renderDebugUI();
void handleKeyboardInput(GLFWwindow* window);

extern Gloom::Camera* camera;
extern bool  gRenderAsPointCloud;
extern int   gSortEveryNFrames;
extern float gCurrentFps;
extern int   gShDegree;