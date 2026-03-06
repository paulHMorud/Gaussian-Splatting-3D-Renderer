#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

#include <utilities/shader.hpp>
#include "utilities/camera.hpp"
#include "utilities/window.hpp"

#include "gaussian.hpp"

enum KeyFrameAction {
    BOTTOM, TOP
};

#include <timestamps.h>


unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;


// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* shader;

Gloom::Camera* camera = new Gloom::Camera(glm::vec3(0.0,0.0, -20.0));

CommandLineOptions options;

// Forward keyboard events to camera
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    camera->handleKeyboardInputs(key, action);
    GLfloat deltaTime = 0.1f;
    camera->updateCamera(deltaTime);
    std::cout << "Ts" << std::endl;
}

void initGame(GLFWwindow* window, CommandLineOptions options)
{
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_DEPTH_TEST);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/gaussian.vert",
                            "../res/shaders/gaussian.frag");

    shader->activate();

    GaussianLoader* loader = new GaussianLoader("../res/bonsai_30000.ply");

    std::vector<GaussianData>* gaussianSplats = loader->getGaussianSplats();

    std::cout << "Loaded splats: "
              << gaussianSplats->size() << std::endl;
}

void updateFrame(GLFWwindow* window) {
    return;
}




void renderFrame(GLFWwindow* window)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->activate();

    int windowWidth, windowHeight; 
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection =
        glm::perspective(glm::radians(80.0f),
        float(windowWidth) / float(windowHeight),
        0.1f,
        500.f);

    glm::mat4 VP = projection * view;

    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(VP));

    // draw gaussian splats here
}
