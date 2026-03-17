#pragma once

#include <GLFW/glfw3.h>

#include <utilities/window.hpp>
#include "utilities/camera.hpp"
#include "sceneGraph.hpp"

void updateNodeTransformations(SceneNode* node, glm::mat4 VP, glm::mat4 modelThusFar);
void initGame(GLFWwindow* window, CommandLineOptions options);
void renderFrame(GLFWwindow* window);
void setTextureUniforms(SceneNode* node, int windowWidth, int windowHeight);

extern Gloom::Camera* camera;