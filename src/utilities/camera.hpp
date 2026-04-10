#define CAMERA_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>


namespace Gloom
{
    /* Handles camera movement and calculation of view matrix*/
    class Camera
    {
    public:
        Camera(glm::vec3 position         = glm::vec3(0.0f, 0.0f, 2.0f),
               GLfloat   movementSpeed    = 2.0f,
               GLfloat   mouseSensitivity = 0.005f)
        {
            cPosition         = position;
            cMovementSpeed    = movementSpeed;
            cMouseSensitivity = mouseSensitivity;

            // Set up the initial view matrix
            updateViewMatrix();
        }

        /* Getter for the view matrix */
        glm::mat4 getViewMatrix() { return matView; }

        /* Getter for camera world position */
        glm::vec3 getPosition() const { return cPosition; }


        /* Handle keyboard inputs from a callback mechanism */
        void handleKeyboardInputs(int key, int action)
        {
            // Keep track of pressed/released buttons
            if (key >= 0 && key < 512) {
                if (action == GLFW_PRESS) {
                    keysInUse[key] = true;
                }
                else if (action == GLFW_RELEASE) {
                    keysInUse[key] = false;
                }
            }
        }


        /* Handle mouse button inputs from a callback mechanism */
        void handleMouseButtonInputs(int button, int action) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                isMousePressed = true;
            }
            else {
                isMousePressed = false;
                resetMouse = true;
            }
        }


        /* Handle cursor position from a callback mechanism */
        void handleCursorPosInput(double xpos, double ypos) {
            if (!isMousePressed)
                return;

            if (resetMouse) {
                lastXPos = xpos;
                lastYPos = ypos;
                resetMouse = false;
                return;
            }

            float deltaYaw   = float(xpos - lastXPos) * cMouseSensitivity;
            float deltaPitch = float(ypos - lastYPos) * cMouseSensitivity;

            cYaw   += deltaYaw;
            cPitch += deltaPitch;
            cPitch  = glm::clamp(cPitch, glm::radians(-89.0f), glm::radians(89.0f));

            lastXPos = float(xpos);
            lastYPos = float(ypos);
        }


        /* Update the camera position and view matrix
           `deltaTime` is the time between the current and last frame */
        void updateCamera(GLfloat deltaTime) {
            glm::vec3 dirX = glm::normalize(cQuaternion * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 dirY(0.0f, 1.0f, 0.0f); // world up
            glm::vec3 dirZ = glm::normalize(cQuaternion * glm::vec3(0.0f, 0.0f, -1.0f));

            glm::vec3 fMovement(0.0f);

            if (keysInUse[GLFW_KEY_W]) fMovement += dirZ;
            if (keysInUse[GLFW_KEY_S]) fMovement -= dirZ;
            if (keysInUse[GLFW_KEY_A]) fMovement -= dirX;
            if (keysInUse[GLFW_KEY_D]) fMovement += dirX;
            if (keysInUse[GLFW_KEY_E]) fMovement += dirY;
            if (keysInUse[GLFW_KEY_Q]) fMovement -= dirY;

            cPitch = glm::clamp(cPitch, glm::radians(-89.0f), glm::radians(89.0f));

            GLfloat velocity = cMovementSpeed * deltaTime;
            cPosition += fMovement * velocity;

            updateViewMatrix();
        }

    private:
        // Disable copying and assignment
        Camera(Camera const &) = delete;
        Camera & operator =(Camera const &) = delete;

        /* Update the view matrix based on the current information */
        void updateViewMatrix() {
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

            // Yaw about global up
            glm::quat qYaw = glm::angleAxis(cYaw, worldUp);

            // Pitch about the camera's right axis after yaw
            glm::vec3 right = glm::normalize(qYaw * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat qPitch = glm::angleAxis(cPitch, right);

            // Camera orientation in world space
            cQuaternion = glm::normalize(qPitch * qYaw);

            // View matrix uses inverse camera transform
            glm::mat4 matRotation = glm::mat4_cast(glm::conjugate(cQuaternion));
            glm::mat4 matTranslate = glm::translate(glm::mat4(1.0f), -cPosition);

            matView = matRotation * matTranslate;
        }

        // Camera quaternion and frame pitch and yaw
        glm::quat cQuaternion;
        GLfloat cPitch = 0.0f;
        GLfloat cYaw   = 0.0f; 

        // Camera position
        glm::vec3 cPosition;

        // Variables used for bookkeeping
        GLboolean resetMouse     = true;
        GLboolean isMousePressed = false;
        GLboolean keysInUse[512] = {false};

        // Rotation speed (radians per second) for keyboard arrow control
        GLfloat cRotationSpeed = 0.4f;

        // Last cursor position
        GLfloat lastXPos = 0.0f;
        GLfloat lastYPos = 0.0f;

        // Camera settings
        GLfloat cMovementSpeed;
        GLfloat cMouseSensitivity;

        // View matrix
        glm::mat4 matView;
    };
}
