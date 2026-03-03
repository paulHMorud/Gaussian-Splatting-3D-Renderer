#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <SFML/Audio/SoundBuffer.hpp>
#include <utilities/shader.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <utilities/timeutils.h>
#include <utilities/mesh.h>
#include <utilities/shapes.h>
#include <utilities/glutils.h>
#include <SFML/Audio/Sound.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "gamelogic.h"
#include "sceneGraph.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "utilities/camera.hpp"

#include "utilities/imageLoader.hpp"
#include "utilities/glfont.h"

#include <pcl/io/ply_io.h>
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>

enum KeyFrameAction {
    BOTTOM, TOP
};

#include <timestamps.h>

int initTexture(PNGImage* im);

double padPositionX = 0;
double padPositionZ = 0;

unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;

SceneNode* rootNode;
SceneNode* boxNode;
SceneNode* ballNode;
SceneNode* padNode;

SceneNode* textureNode;

const int NUM_LIGHTS = 3;

double ballRadius = 3.0f;

// These are heap allocated, because they should not be initialised at the start of the program
sf::SoundBuffer* buffer;
Gloom::Shader* shader;
Gloom::Shader* textureShader;
sf::Sound* sound;

Gloom::Camera* camera = new Gloom::Camera(glm::vec3(0.0,0.0, -20.0));

PNGImage charMap;
PNGImage debugMap;
PNGImage brickDiffuse;
PNGImage brickNormal;
PNGImage brickRough;

const glm::vec3 boxDimensions(180, 90, 90);
const glm::vec3 padDimensions(30, 3, 40);

glm::vec3 ballPosition(0, ballRadius + padDimensions.y, boxDimensions.z / 2);
glm::vec3 ballDirection(1, 1, 0.2f);

CommandLineOptions options;

bool hasStarted        = false;
bool hasLost           = false;
bool jumpedToNextFrame = false;
bool isPaused          = false;

bool mouseLeftPressed   = false;
bool mouseLeftReleased  = false;
bool mouseRightPressed  = false;
bool mouseRightReleased = false;

// Modify if you want the music to start further on in the track. Measured in seconds.
const float debug_startTime = 0;
double totalElapsedTime = debug_startTime;
double gameElapsedTime = debug_startTime;

double mouseSensitivity = 1.0;
double lastMouseX = windowWidth / 2;
double lastMouseY = windowHeight / 2;

void mouseCallback(GLFWwindow* window, double x, double y) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    padPositionX -= mouseSensitivity * deltaX / windowWidth;
    padPositionZ -= mouseSensitivity * deltaY / windowHeight;

    if (padPositionX > 1) padPositionX = 1;
    if (padPositionX < 0) padPositionX = 0;
    if (padPositionZ > 1) padPositionZ = 1;
    if (padPositionZ < 0) padPositionZ = 0;

    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
}

// Forward keyboard events to camera
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    camera->handleKeyboardInputs(key, action);
    GLfloat deltaTime = 0.1f;
    camera->updateCamera(deltaTime);
}

//// A few lines to help you if you've never used c++ structs
// struct LightSource {
//     bool a_placeholder_value;
// };
// LightSource lightSources[/*Put number of light sources you want here*/];

struct LightSource {
    SceneNode* node;
    glm::vec3 color;
    glm::vec3 worldPosition;
};
LightSource lightSources[3];



void initGame(GLFWwindow* window, CommandLineOptions gameOptions) {
    buffer = new sf::SoundBuffer();
    if (!buffer->loadFromFile("../res/Hall of the Mountain King.ogg")) {
        return;
    }

    // std::string filePath = "../res/textures/charmap.png"; 
    // charMap = loadPNGFile(filePath);
    
    // std::cout << "Charmap loaded: " << charMap.width << "x" << charMap.height << std::endl;
    
    // if (charMap.width == 0 || charMap.height == 0) {
    //     std::cerr << "ERROR: Failed to load charmap texture!" << std::endl;
    //     return;
    // }

    // filePath = "../res/textures/normal-map-debug.png"; 
    // debugMap = loadPNGFile(filePath);
    
    // std::cout << "DebugMap loaded: " << debugMap.width << "x" << debugMap.height << std::endl;
    
    // if (debugMap.width == 0 || debugMap.height == 0) {
    //     std::cerr << "ERROR: Failed to load debugMap texture!" << std::endl;
    //     return;
    // }
    // int debugMapID = initTexture(&debugMap);

    // filePath = "../res/textures/Brick03_col.png";
    // brickDiffuse = loadPNGFile(filePath);
    // std::cout << "Brick diffuse loaded: " << brickDiffuse.width << "x" << brickDiffuse.height << std::endl;
    // if (brickDiffuse.width == 0 || brickDiffuse.height == 0) {
    //     std::cerr << "ERROR: Failed to load Brick03_col.png!" << std::endl;
    //     return;
    // }
    // int brickDiffuseID = initTexture(&brickDiffuse);

    // filePath = "../res/textures/Brick03_nrm.png";
    // brickNormal = loadPNGFile(filePath);
    // std::cout << "Brick normal loaded: " << brickNormal.width << "x" << brickNormal.height << std::endl;
    // if (brickNormal.width == 0 || brickNormal.height == 0) {
    //     std::cerr << "ERROR: Failed to load Brick03_nrm.png!" << std::endl;
    //     return;
    // }
    // int brickNormalID = initTexture(&brickNormal);

    // filePath = "../res/textures/Brick03_rgh.png";
    // brickRough = loadPNGFile(filePath);
    // std::cout << "Brick roughness loaded: " << brickRough.width << "x" << brickRough.height << std::endl;
    // if (brickRough.width == 0 || brickRough.height == 0) {
    //     std::cerr << "ERROR: Failed to load Brick03_rgh.png!" << std::endl;
    //     return;
    // }
    // int brickRoughID = initTexture(&brickRough);

    options = gameOptions;

    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    glfwSetKeyCallback(window, keyCallback);

    glEnable(GL_DEPTH_TEST);


    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader->activate();

    textureShader = new Gloom::Shader();
    textureShader->makeBasicShader("../res/shaders/texture.vert", "../res/shaders/texture.frag");

    // Creating mesh and vao buffer for texture and creating textureNode
    // int textureID = initTexture(&charMap);
    // std::string test_string = "Writing text is for cool people!";
    // Mesh texture = generateTextGeometryBuffer(test_string, 39.0f/29.0f, 128.0f*10.0f);
    // unsigned int textureVAO = generateBuffer(texture);
    // textureNode = createSceneNode();

    // Create meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);
    Mesh sphere = generateSphere(1.0, 40, 40);

    // Fill buffers
    unsigned int ballVAO = generateBuffer(sphere);
    unsigned int boxVAO  = generateBuffer(box);
    unsigned int padVAO  = generateBuffer(pad);

    // Construct scene
    rootNode = createSceneNode();
    boxNode  = createSceneNode();
    padNode  = createSceneNode();
    ballNode = createSceneNode();

    

    // Lightsources
    for (int i = 0; i < 3; i++) {
        lightSources[i].node = createSceneNode();
        lightSources[i].node->nodeType = POINT_LIGHT;
        lightSources[i].node->lightID = i;
    }

    // Red
    lightSources[0].node->position = glm::vec3(0.0f, 10.0f, -80.0f);
    lightSources[0].color = glm::vec3(1.0f, 0.0f, 0.0f); 
    
    // green
    lightSources[1].node->position = glm::vec3(0.0f, 10.0f, -80.0f);
    lightSources[1].color = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // blue light attached to ball
    lightSources[2].node->position = glm::vec3(0.0f, 10.0f, -80.0f); 
    lightSources[2].color = glm::vec3(0.0f, 0.0f, 1.0f);


    rootNode->children.push_back(boxNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(ballNode);
    // Adding lightsources to scenegraph
    rootNode->children.push_back(lightSources[0].node);
    rootNode->children.push_back(lightSources[1].node);
    rootNode->children.push_back(lightSources[2].node); 

    // // Adding texture node to root
    // rootNode->children.push_back(textureNode);

    // textureNode->nodeType = GEOMETRY2D;
    // textureNode->textureID = textureID;
    // textureNode->vertexArrayObjectID  = textureVAO;
    // textureNode->VAOIndexCount        = texture.indices.size();

    boxNode->vertexArrayObjectID  = boxVAO;
    boxNode->VAOIndexCount        = box.indices.size();
    boxNode->nodeType = GEOMETRY;
    // boxNode->textureID = brickDiffuseID;   
    // boxNode->normalMapID = brickNormalID;     
    // boxNode->roughnessMapID = brickRoughID;   
    

    padNode->vertexArrayObjectID  = padVAO;
    padNode->VAOIndexCount        = pad.indices.size();

    ballNode->vertexArrayObjectID = ballVAO;
    ballNode->VAOIndexCount       = sphere.indices.size();


    getTimeDeltaSeconds();

    // std::cout << "box vertex count: " << box.vertices.size()
    //           << " box texcoord count: " << box.textureCoordinates.size() << std::endl;
    // std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;

    // std::cout << "Ready. Click to start!" << std::endl;
}

void updateFrame(GLFWwindow* window) {
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double timeDelta = getTimeDeltaSeconds();

    const float ballBottomY = boxNode->position.y - (boxDimensions.y/2) + ballRadius + padDimensions.y;
    const float ballTopY    = boxNode->position.y + (boxDimensions.y/2) - ballRadius;
    const float BallVerticalTravelDistance = ballTopY - ballBottomY;

    

    const float ballMinX = boxNode->position.x - (boxDimensions.x/2) + ballRadius;
    const float ballMaxX = boxNode->position.x + (boxDimensions.x/2) - ballRadius;
    const float ballMinZ = boxNode->position.z - (boxDimensions.z/2) + ballRadius;
    const float ballMaxZ = boxNode->position.z + (boxDimensions.z/2) - ballRadius;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) {
        mouseLeftPressed = true;
        mouseLeftReleased = false;
    } else {
        mouseLeftReleased = mouseLeftPressed;
        mouseLeftPressed = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2)) {
        mouseRightPressed = true;
        mouseRightReleased = false;
    } else {
        mouseRightReleased = mouseRightPressed;
        mouseRightPressed = false;
    }

    if(!hasStarted) {
        if (mouseLeftPressed) {
            if (options.enableMusic) {
                sound = new sf::Sound();
                sound->setBuffer(*buffer);
                sf::Time startTime = sf::seconds(debug_startTime);
                sound->setPlayingOffset(startTime);
                sound->play();
            }
            totalElapsedTime = debug_startTime;
            gameElapsedTime = debug_startTime;
            hasStarted = true;
        }

        ballPosition.x = ballMinX + (1 - padPositionX) * (ballMaxX - ballMinX);
        ballPosition.y = ballBottomY;
        ballPosition.z = ballMinZ + (1 - padPositionZ) * ((ballMaxZ) - ballMinZ);
    } else {
        totalElapsedTime += timeDelta;
        if(hasLost) {
            if (mouseLeftReleased) {
                hasLost = false;
                hasStarted = false;
                currentKeyFrame = 0;
                previousKeyFrame = 0;
            }
        } else if (isPaused) {
            if (mouseRightReleased) {
                isPaused = false;
                if (options.enableMusic) {
                    sound->play();
                }
            }
        } else {
            gameElapsedTime += timeDelta;
            if (mouseRightReleased) {
                isPaused = true;
                if (options.enableMusic) {
                    sound->pause();
                }
            }
            // Get the timing for the beat of the song
            for (unsigned int i = currentKeyFrame; i < keyFrameTimeStamps.size(); i++) {
                if (gameElapsedTime < keyFrameTimeStamps.at(i)) {
                    continue;
                }
                currentKeyFrame = i;
            }

            jumpedToNextFrame = currentKeyFrame != previousKeyFrame;
            previousKeyFrame = currentKeyFrame;

            double frameStart = keyFrameTimeStamps.at(currentKeyFrame);
            double frameEnd = keyFrameTimeStamps.at(currentKeyFrame + 1); // Assumes last keyframe at infinity

            double elapsedTimeInFrame = gameElapsedTime - frameStart;
            double frameDuration = frameEnd - frameStart;
            double fractionFrameComplete = elapsedTimeInFrame / frameDuration;

            double ballYCoord;

            KeyFrameAction currentOrigin = keyFrameDirections.at(currentKeyFrame);
            KeyFrameAction currentDestination = keyFrameDirections.at(currentKeyFrame + 1);

            // Synchronize ball with music
            if (currentOrigin == BOTTOM && currentDestination == BOTTOM) {
                ballYCoord = ballBottomY;
            } else if (currentOrigin == TOP && currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance;
            } else if (currentDestination == BOTTOM) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * (1 - fractionFrameComplete);
            } else if (currentDestination == TOP) {
                ballYCoord = ballBottomY + BallVerticalTravelDistance * fractionFrameComplete;
            }

            // Make ball move
            const float ballSpeed = 60.0f;
            ballPosition.x += timeDelta * ballSpeed * ballDirection.x;
            ballPosition.y = ballYCoord;
            ballPosition.z += timeDelta * ballSpeed * ballDirection.z;

            // Make ball bounce
            if (ballPosition.x < ballMinX) {
                ballPosition.x = ballMinX;
                ballDirection.x *= -1;
            } else if (ballPosition.x > ballMaxX) {
                ballPosition.x = ballMaxX;
                ballDirection.x *= -1;
            }
            if (ballPosition.z < ballMinZ) {
                ballPosition.z = ballMinZ;
                ballDirection.z *= -1;
            } else if (ballPosition.z > ballMaxZ) {
                ballPosition.z = ballMaxZ;
                ballDirection.z *= -1;
            }

            if(options.enableAutoplay) {
                padPositionX = 1-(ballPosition.x - ballMinX) / (ballMaxX - ballMinX);
                padPositionZ = 1-(ballPosition.z - ballMinZ) / ((ballMaxZ) - ballMinZ);
            }

            // Check if the ball is hitting the pad when the ball is at the bottom.
            // If not, you just lost the game! (hehe)
            if (jumpedToNextFrame && currentOrigin == BOTTOM && currentDestination == TOP) {
                double padLeftX  = boxNode->position.x - (boxDimensions.x/2) + (1 - padPositionX) * (boxDimensions.x - padDimensions.x);
                double padRightX = padLeftX + padDimensions.x;
                double padFrontZ = boxNode->position.z - (boxDimensions.z/2) + (1 - padPositionZ) * (boxDimensions.z - padDimensions.z);
                double padBackZ  = padFrontZ + padDimensions.z;

                if (   ballPosition.x < padLeftX
                    || ballPosition.x > padRightX
                    || ballPosition.z < padFrontZ
                    || ballPosition.z > padBackZ
                ) {
                    hasLost = true;
                    if (options.enableMusic) {
                        sound->stop();
                        delete sound;
                    }
                }
            }
        }
    }

    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(windowWidth) / float(windowHeight), 0.1f, 350.f);

    glm::vec3 cameraPosition = camera->getPosition();

    // Some math to make the camera move in a nice way
    float lookRotation = -0.6 / (1 + exp(-5 * (padPositionX-0.5))) + 0.3;
    glm::mat4 cameraTransform = camera->getViewMatrix();
                    // glm::rotate(0.3f + 0.2f * float(-padPositionZ*padPositionZ), glm::vec3(1, 0, 0)) *
                    // glm::rotate(lookRotation, glm::vec3(0, 1, 0)) *
                    // glm::translate(-cameraPosition);

    glm::mat4 VP = projection * cameraTransform;

    // Move and rotate various SceneNodes
    boxNode->position = { 0, -10, -80 };

    ballNode->position = ballPosition;
    ballNode->scale = glm::vec3(ballRadius);
    ballNode->rotation = { 0, totalElapsedTime*2, 0 };

    padNode->position  = {
        boxNode->position.x - (boxDimensions.x/2) + (padDimensions.x/2) + (1 - padPositionX) * (boxDimensions.x - padDimensions.x),
        boxNode->position.y - (boxDimensions.y/2) + (padDimensions.y/2),
        boxNode->position.z - (boxDimensions.z/2) + (padDimensions.z/2) + (1 - padPositionZ) * (boxDimensions.z - padDimensions.z)
    };

    updateNodeTransformations(rootNode, VP, glm::mat4(1.0f));




}

void updateNodeTransformations(SceneNode* node, glm::mat4 VP, glm::mat4 modelThusFar) {
    if (node->nodeType == GEOMETRY2D) {
        node->modelMatrix = glm::mat4(1.0f);
        node->currentTransformationMatrix = glm::mat4(1.0f); // Not used for 2D
        
        for(SceneNode* child : node->children) {
            updateNodeTransformations(child, VP, glm::mat4(1.0f));
        }
        return;
    }
    
    glm::mat4 localTransform =
              glm::translate(node->position)
            * glm::translate(node->referencePoint)
            * glm::rotate(node->rotation.y, glm::vec3(0,1,0))
            * glm::rotate(node->rotation.x, glm::vec3(1,0,0))
            * glm::rotate(node->rotation.z, glm::vec3(0,0,1))
            * glm::scale(node->scale)
            * glm::translate(-node->referencePoint);

    node->modelMatrix = modelThusFar * localTransform;
    node->currentTransformationMatrix = VP * node->modelMatrix;

    if (node->nodeType == POINT_LIGHT && node->lightID >= 0 && node->lightID < NUM_LIGHTS) {
        // Transforming [0,0,0,1] by the model matrix to get position
        glm::vec4 origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 worldPosition = node->modelMatrix * origin;
        
        // Storing light position
        lightSources[node->lightID].worldPosition = glm::vec3(worldPosition) / worldPosition.w;
    }


    switch(node->nodeType) {
        case GEOMETRY: break;
        case POINT_LIGHT: break;
        case SPOT_LIGHT: break;
        case NORMALMAP: break;
    }

    for(SceneNode* child : node->children) {
        updateNodeTransformations(child, VP, node->modelMatrix);
    }
}

void setLightUniforms() {
    // Set number of lights
    glUniform1i(glGetUniformLocation(shader->get(), "numLights"), NUM_LIGHTS);
    
    // Set light positions and colors
    for (int i = 0; i < NUM_LIGHTS; i++) {
        // std::string posName = "lightPositions[" + std::to_string(i) + "]";
        // std::string colorName = "lightColors[" + std::to_string(i) + "]";

        std::string lightName = "lights[" + std::to_string(i) + "]";

        std::string posName = lightName + ".position";
        std::string colorName = lightName + ".color";
        
        GLint posLoc = glGetUniformLocation(shader->get(), posName.c_str());
        GLint colorLoc = glGetUniformLocation(shader->get(), colorName.c_str());
        
        glUniform3fv(posLoc, 1, glm::value_ptr(lightSources[i].worldPosition));
        glUniform3fv(colorLoc, 1, glm::value_ptr(lightSources[i].color));
    }
}

void setBallUniforms() {
    glUniform3fv(glGetUniformLocation(shader->get(), "ballPosition"), 1, glm::value_ptr(ballNode->position));
}

int initTexture(PNGImage* im) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        im->width, im->height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        im->pixels.data()
    );
    
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    return id;
}

void renderNode(SceneNode* node) {
    if (node->nodeType == GEOMETRY2D) return;

    glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(node->currentTransformationMatrix));

    // Pass Model matrix at location 4
    glUniformMatrix4fv(4, 1, GL_FALSE, glm::value_ptr(node->modelMatrix));

    glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->modelMatrix)));
    glUniformMatrix3fv(6, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    switch(node->nodeType) {
        case GEOMETRY:
            if(node->vertexArrayObjectID != -1) {
                glUniform1i(glGetUniformLocation(shader->get(), "useNormalMap"), 0);

                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case NORMALMAP:
            if (node->vertexArrayObjectID != -1) {
                glUniform1i(glGetUniformLocation(shader->get(), "useNormalMap"), 1);

                // Bind material textures to texture units
                glBindTextureUnit(0, node->textureID);       // diffuse
                glBindTextureUnit(1, node->normalMapID);     // normal
                glBindTextureUnit(2, node->roughnessMapID);  // roughness

                // Setting texture uniforms
                glUniform1i(glGetUniformLocation(shader->get(), "diffuseMap"), 0);
                glUniform1i(glGetUniformLocation(shader->get(), "normalMap"), 1);
                glUniform1i(glGetUniformLocation(shader->get(), "roughnessMap"), 2);

                glBindVertexArray(node->vertexArrayObjectID);
                glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            }
            break;
        case POINT_LIGHT: break;
        case SPOT_LIGHT: break;
        case GEOMETRY2D: break;
    }

    for(SceneNode* child : node->children) {
        renderNode(child);
    }
}

void render2DNode(SceneNode* node) {
    switch(node->nodeType) {
        case GEOMETRY: break;
        case POINT_LIGHT: break;
        case SPOT_LIGHT: break;
        case GEOMETRY2D:
            setTextureUniforms(node, windowWidth, windowHeight);
            glBindVertexArray(node->vertexArrayObjectID);
            glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
            break;
        case NORMALMAP: break;
    }

    for(SceneNode* child : node->children) {
        render2DNode(child);
    }
}

void setTextureUniforms(SceneNode* node, int windowWidth, int windowHeight) {
    glm::mat4 ortho = glm::ortho(
        0.0f, float(windowWidth), 
        0.0f, float(windowHeight)
    );
    
    glm::mat4 translation = glm::translate(glm::vec3(50.0f, windowHeight - 100.0f, 0.0f));
    glm::mat4 MVP = ortho * translation;
    // glm::mat4 MVP = ortho * node->modelMatrix;
    
    GLint mvpLoc = glGetUniformLocation(textureShader->get(), "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));
    
    // Bind texture to unit 0 (OpenGL 4.5+)
    glBindTextureUnit(0, node->textureID);
}


void renderFrame(GLFWwindow* window) {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    shader->activate();
    setLightUniforms();
    setBallUniforms();
    renderNode(rootNode);
}
