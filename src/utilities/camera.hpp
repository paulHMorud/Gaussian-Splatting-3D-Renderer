#define CAMERA_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>


namespace Gloom
{
    /* Handles camera movement and calculation of view matrix */
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

            updateViewMatrix();
        }

        /* Getter for the view matrix */
        glm::mat4 getViewMatrix() { return matView; }

        /* Getter for camera world position */
        glm::vec3 getPosition() const { return cPosition; }

        // ------------------------------------------------------------------
        // setOrbitPose – used by the orbit recorder in program.cpp.
        // Directly sets the camera's world position, yaw, and pitch, then
        // rebuilds the view matrix.  Does NOT touch keyboard/mouse state so
        // it is safe to call every frame from the render loop.
        // ------------------------------------------------------------------
        void setOrbitPose(glm::vec3 position, float yaw, float pitch)
        {
            cPosition = position;
            cYaw      = yaw;
            cPitch    = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));
            updateViewMatrix();
        }

        /* Handle keyboard inputs from a callback mechanism */
        void handleKeyboardInputs(int key, int action)
        {
            if (key >= 0 && key < 512) {
                if (action == GLFW_PRESS)        keysInUse[key] = true;
                else if (action == GLFW_RELEASE) keysInUse[key] = false;
            }
        }

        /* Handle mouse button inputs from a callback mechanism */
        void handleMouseButtonInputs(int button, int action) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                isMousePressed = true;
            } else {
                isMousePressed = false;
                resetMouse     = true;
            }
        }

        /* Handle cursor position from a callback mechanism */
        void handleCursorPosInput(double xpos, double ypos) {
            if (!isMousePressed) return;

            if (resetMouse) {
                lastXPos   = float(xpos);
                lastYPos   = float(ypos);
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

        /* Update the camera position and view matrix each frame */
        void updateCamera(GLfloat deltaTime) {
            glm::vec3 dirX = glm::normalize(cQuaternion * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 dirY(0.0f, 1.0f, 0.0f);
            glm::vec3 dirZ = glm::normalize(cQuaternion * glm::vec3(0.0f, 0.0f, -1.0f));

            glm::vec3 fMovement(0.0f);
            if (keysInUse[GLFW_KEY_W]) fMovement += dirZ;
            if (keysInUse[GLFW_KEY_S]) fMovement -= dirZ;
            if (keysInUse[GLFW_KEY_A]) fMovement -= dirX;
            if (keysInUse[GLFW_KEY_D]) fMovement += dirX;
            if (keysInUse[GLFW_KEY_E]) fMovement += dirY;
            if (keysInUse[GLFW_KEY_Q]) fMovement -= dirY;

            cPitch = glm::clamp(cPitch, glm::radians(-89.0f), glm::radians(89.0f));

            cPosition += fMovement * (cMovementSpeed * deltaTime);
            updateViewMatrix();
        }

    private:
        Camera(Camera const &)            = delete;
        Camera & operator=(Camera const &) = delete;

        void updateViewMatrix() {
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

            glm::quat qYaw   = glm::angleAxis(cYaw, worldUp);
            glm::vec3 right  = glm::normalize(qYaw * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat qPitch = glm::angleAxis(cPitch, right);

            cQuaternion = glm::normalize(qPitch * qYaw);

            glm::mat4 matRotation  = glm::mat4_cast(glm::conjugate(cQuaternion));
            glm::mat4 matTranslate = glm::translate(glm::mat4(1.0f), -cPosition);
            matView = matRotation * matTranslate;
        }

        glm::quat cQuaternion;
        GLfloat   cPitch = 0.0f;
        GLfloat   cYaw   = 0.0f;

        glm::vec3 cPosition;

        GLboolean resetMouse     = true;
        GLboolean isMousePressed = false;
        GLboolean keysInUse[512] = {false};

        GLfloat   cRotationSpeed    = 0.4f;
        GLfloat   lastXPos          = 0.0f;
        GLfloat   lastYPos          = 0.0f;
        GLfloat   cMovementSpeed;
        GLfloat   cMouseSensitivity;

        glm::mat4 matView;
    };
}