#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

#include "imgui.h"

#include <utilities/shader.hpp>
#include "utilities/window.hpp"
#include "utilities/glutils.h"

#include "gamelogic.h"
#include "gaussian.hpp"
#include "utilities/radix_sort.hpp"

enum KeyFrameAction {
    BOTTOM, TOP
};

#include <timestamps.h>


unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;


Gloom::Shader* shader;
GaussianBuffers gaussianBuffers;
GaussianLoader* loader;

Gloom::Camera* camera = new Gloom::Camera(glm::vec3(0.0,0.0, 0.0));

std::vector<GaussianData> gaussianSplats;

RadixSort gRadixSort;

CommandLineOptions options;

GLuint viewLocation;
GLuint projLocation;
GLuint isPointCloudLocation;
GLuint tanHalfFovLocation;
GLuint focalLengthLocation;
GLuint shDegreeLocation;
GLuint cameraPosLocation;

unsigned int counter = 0;
float fieldOfView = 60.0f;
bool  gRenderAsPointCloud = false; //Toggling point cloud on and off
int   gSortEveryNFrames = 1; //Telling the program how many frames between sort (and culling)
float gCurrentFps = 0.0f;
int   gShDegree = 3; //degree of spherical harmonics
static bool gUseSH = true; //toggling SH on and off
static uint32_t gLastVisibleCount = 0;


//Closes the program if esc is pressed, sends input to camera
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        delete loader;
    }

    camera->handleKeyboardInputs(key, action);
}


//Setting up OpenGL settings and activating shaders. 
void initGame(GLFWwindow* window, CommandLineOptions options)
{
    glfwSetKeyCallback(window, keyCallback);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods){
        camera->handleMouseButtonInputs(button, action);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos){
        camera->handleCursorPosInput(xpos, ypos);
    });

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    glEnable(GL_CULL_FACE);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/gaussian.vert",
                            "../res/shaders/gaussian.frag");

    shader->activate();

    loader = new GaussianLoader("../res/flowers.ply");

    gaussianSplats = loader->getGaussianSplats();

    std::cout << "Loaded splats: " << gaussianSplats.size() << std::endl;
    std::cout << "sizeof(GaussianData): " << sizeof(GaussianData) << std::endl;

    gaussianBuffers = generateGaussianBuffer(gaussianSplats);

    gRadixSort.init((uint32_t)gaussianSplats.size());

    viewLocation         = glGetUniformLocation(shader->get(), "viewMatrix");
    projLocation         = glGetUniformLocation(shader->get(), "projectionMatrix");
    isPointCloudLocation = glGetUniformLocation(shader->get(), "isPointCloud");
    tanHalfFovLocation   = glGetUniformLocation(shader->get(), "tanHalfFov");
    focalLengthLocation  = glGetUniformLocation(shader->get(), "focalLength");
    shDegreeLocation     = glGetUniformLocation(shader->get(), "uShDegree");
    cameraPosLocation    = glGetUniformLocation(shader->get(), "uCameraPos");

    static_assert(sizeof(GPUGaussian) == 240, "GPUGaussian must be 240 bytes (with SH)");
}


void renderDebugUI()
{
    ImGui::Begin("Renderer");

    ImGui::Checkbox("Render as point cloud", &gRenderAsPointCloud);
    ImGui::SliderInt("Sort every N frames", &gSortEveryNFrames, 0, 120);
    ImGui::Text("FPS: %.1f", gCurrentFps);

    if (ImGui::Checkbox("Spherical harmonics (view-dependent color)", &gUseSH)) {
        gShDegree = gUseSH ? 3 : 0;
    }
    if (gUseSH) {
        ImGui::SliderInt("SH degree", &gShDegree, 0, 3);
    } else {
        ImGui::Text("SH disabled (DC only — flat color)");
    }

    if (gSortEveryNFrames == 0) {
        ImGui::Text("Sorting: disabled");
    } else {
        ImGui::Text("Sorting: every %d frames", gSortEveryNFrames);
    }

    glm::vec3 p = camera->getPosition();
    ImGui::Text("Cam pos: %.2f  %.2f  %.2f", p.x, p.y, p.z);
    ImGui::Text("Splat count: %zu", gaussianSplats.size());

    ImGui::End();
}

void renderPointCloud(size_t splatCount) {
    glUniform1i(isPointCloudLocation, 1);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArraysInstanced(GL_POINTS, 0, 1, splatCount);
}

void renderFrame(GLFWwindow* window)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection =
        glm::perspective(glm::radians(fieldOfView),
        float(windowWidth) / float(windowHeight),
        0.1f,
        200.f);

    float tanHalfY = tanf(glm::radians(fieldOfView) * 0.5f);
    float tanHalfX = tanHalfY * (float(windowWidth) / float(windowHeight));
    float focal = float(windowHeight) / (2.0f * tanHalfY);

    if (gSortEveryNFrames > 0 && counter % gSortEveryNFrames == 0) {
        gLastVisibleCount = gRadixSort.sort(view, projection,
                                            gaussianBuffers.ssbo,
                                            (uint32_t)gaussianSplats.size());
    }
    counter++;

    shader->activate();

    glm::vec3 camPos = camera->getPosition();

    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform2f(tanHalfFovLocation, tanHalfX, tanHalfY);
    glUniform1f(focalLengthLocation, focal);
    glUniform1i(shDegreeLocation, gShDegree);
    glUniform3f(cameraPosLocation, camPos.x, camPos.y, camPos.z);

    glBindVertexArray(gaussianBuffers.vao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianBuffers.ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gRadixSort.sortedIndexBuffer());

    if (gRenderAsPointCloud) {
        renderPointCloud(static_cast<GLsizei>(gLastVisibleCount));
    } else {
        glUniform1i(isPointCloudLocation, 0);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(gLastVisibleCount));
    }
}