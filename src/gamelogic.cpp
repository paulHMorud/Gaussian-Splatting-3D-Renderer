#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

#include <utilities/shader.hpp>
#include "utilities/window.hpp"
#include "utilities/glutils.h"

#include "gamelogic.h"

#include "gaussian.hpp"

enum KeyFrameAction {
    BOTTOM, TOP
};

#include <timestamps.h>


unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;


// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader* shader;
GaussianBuffers gaussianBuffers;
GaussianLoader* loader;

Gloom::Camera* camera = new Gloom::Camera(glm::vec3(0.0,0.0, 3.0));

std::vector<GaussianData> gaussianSplats;

CommandLineOptions options;

GLuint viewLocation;
GLuint projLocation;
GLuint isPointCloudLocation;
GLuint tanHalfFovLocation;
GLuint focalLengthLocation;

unsigned int counter = 0;
float fieldOfView = 60.0f;


// Forward keyboard events to camera
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        delete loader;
    }

    camera->handleKeyboardInputs(key, action);
    GLfloat deltaTime = 0.1f;
    // camera->updateCamera(deltaTime);

    glm::vec3 cpos = camera->getPosition();
    std::cout << "the camera position is: " << cpos.x << "  " <<  cpos.y << "  " << cpos.z << std::endl;
}

void initGame(GLFWwindow* window, CommandLineOptions options)
{
  
    glfwSetKeyCallback(window, keyCallback);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods){
        camera->handleMouseButtonInputs(button, action);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos){
        camera->handleCursorPosInput(xpos, ypos);
    });

    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    // glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    // glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/gaussian.vert",
                            "../res/shaders/gaussian.frag");

    shader->activate();

    // loader = new GaussianLoader("../res/Scenes/Scenes/truck/point_cloud/iteration_30000/point_cloud.ply");
    loader = new GaussianLoader("../res/bonsai_30000.ply");

    gaussianSplats = loader->getGaussianSplats();

    std::cout << "Loaded splats: "
              << gaussianSplats.size() << std::endl;

    std::cout << sizeof(GaussianData) << std::endl;
    // Setting up VBO and EBO for quads and sending the gaussian splat data to shader
    gaussianBuffers = generateGaussianBuffer(gaussianSplats);

    for (int i = 0; i < 10; i++) {
    auto& g = gaussianSplats[i];
    std::cout << g.position.x << " "
              << g.position.y << " "
              << g.position.z << std::endl;
    std::cout << "Scale x value: " << g.scale.x << std::endl;
    
    }

    viewLocation = glGetUniformLocation(shader->get(), "viewMatrix");
    projLocation = glGetUniformLocation(shader->get(), "projectionMatrix");
    isPointCloudLocation = glGetUniformLocation(shader->get(), "isPointCloud");
    tanHalfFovLocation = glGetUniformLocation(shader->get(), "tanHalfFov");
    focalLengthLocation = glGetUniformLocation(shader->get(), "focalLength");
    
}


// Only renders point cloud
void renderPointCloud(size_t splatCount) {
    glUniform1i(isPointCloudLocation, 1);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArraysInstanced(GL_POINTS, 0, 1, splatCount);

}



void renderFrame(GLFWwindow* window)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->activate();

    int windowWidth, windowHeight; 
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection =
        glm::perspective(glm::radians(fieldOfView),
        float(windowWidth) / float(windowHeight),
        0.1f,
        500.f);


    float tanHalfY = tanf(glm::radians(fieldOfView) * 0.5f);
    float tanHalfX = tanHalfY * (float(windowWidth) / float(windowHeight));
    float focal = float(windowHeight) / (2.0f * tanHalfY);

    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform2f(tanHalfFovLocation, tanHalfX, tanHalfY);
    glUniform1f(focalLengthLocation, focal);


    if (counter++ % 1 == 0) {
        sortGaussiansBackToFront(gaussianBuffers, view);
        updateGaussianSSBO(gaussianBuffers);
    }



    glBindVertexArray(gaussianBuffers.vao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianBuffers.ssbo);


    glUniform1i(isPointCloudLocation, 0);

    // renderPointCloud(gaussianSplats.size());
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, static_cast<GLsizei>(gaussianSplats.size()));
}
