#pragma once

#include <GLFW/glfw3.h>

#include <utilities/window.hpp>
#include "utilities/camera.hpp"

struct CommandLineOptions;

void initGame(GLFWwindow* window, CommandLineOptions options);
void renderFrame(GLFWwindow* window);
void renderDebugUI();

extern Gloom::Camera* camera;

extern bool  gRenderAsPointCloud;
extern int   gSortEveryNFrames;
extern float gCurrentFps;
extern int   gShDegree;