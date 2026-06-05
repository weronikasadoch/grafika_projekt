#include "Scene.h"

#include <GLFW/glfw3.h>

Scene::Scene(int width, int height)
    : width_(width),
      height_(height)
{
}

void Scene::updateFramebufferSize(int width, int height)
{
    width_ = width;
    height_ = height;
}

void Scene::processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    const float rotationVelocity = kCameraRotationSpeed * deltaTime_;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        camera_.rotateYaw(-rotationVelocity);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        camera_.rotateYaw(rotationVelocity);
    }

    const float velocity = kCameraSpeed * deltaTime_;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera_.moveForward(velocity);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera_.moveForward(-velocity);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera_.moveRight(-velocity);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera_.moveRight(velocity);
    }
}

void Scene::updateDeltaTime(float currentFrameTime)
{
    deltaTime_ = currentFrameTime - lastFrameTime_;
    lastFrameTime_ = currentFrameTime;
}

Camera& Scene::getCamera()
{
    return camera_;
}

const Camera& Scene::getCamera() const
{
    return camera_;
}

float Scene::getAspectRatio() const
{
    return static_cast<float>(width_) / static_cast<float>(height_ == 0 ? 1 : height_);
}
