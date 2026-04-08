#pragma once

#include <GLFW/glfw3.h>

#include <utilities/window.hpp>
#include "utilities/camera.hpp"
#include "sceneGraph.hpp"

struct CommandLineOptions;

void updateNodeTransformations(SceneNode* node, glm::mat4 VP, glm::mat4 modelThusFar);
void initGame(GLFWwindow* window, CommandLineOptions options);
void renderFrame(GLFWwindow* window);
void setTextureUniforms(SceneNode* node, int windowWidth, int windowHeight);
void renderDebugUI();

extern Gloom::Camera* camera;

extern bool gRenderAsPointCloud;
extern int gSortEveryNFrames;
extern float gCurrentFps;