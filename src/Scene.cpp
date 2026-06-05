#include "Scene.h"

#include <GLFW/glfw3.h>

#include <algorithm>

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
    updateOutlineSlider(window);

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

float Scene::getFramebufferWidth() const
{
    return static_cast<float>(width_);
}

float Scene::getFramebufferHeight() const
{
    return static_cast<float>(height_);
}

float Scene::getOutlineThickness() const
{
    return outlineThickness_;
}

float Scene::getOutlineSliderValue() const
{
    return (outlineThickness_ - kOutlineMinThickness) / (kOutlineMaxThickness - kOutlineMinThickness);
}

void Scene::updateOutlineSlider(GLFWwindow* window)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
    {
        isDraggingOutlineSlider_ = false;
        return;
    }

    double cursorWindowX = 0.0;
    double cursorWindowY = 0.0;
    glfwGetCursorPos(window, &cursorWindowX, &cursorWindowY);

    int windowWidth = 1;
    int windowHeight = 1;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    const float framebufferX = static_cast<float>(cursorWindowX) * static_cast<float>(width_) / static_cast<float>(windowWidth == 0 ? 1 : windowWidth);
    const float framebufferY = static_cast<float>(cursorWindowY) * static_cast<float>(height_) / static_cast<float>(windowHeight == 0 ? 1 : windowHeight);

    const bool isOverSlider =
        framebufferX >= kOutlineSliderX &&
        framebufferX <= kOutlineSliderX + kOutlineSliderWidth &&
        framebufferY >= kOutlineSliderY &&
        framebufferY <= kOutlineSliderY + kOutlineSliderHeight;

    if (!isDraggingOutlineSlider_ && !isOverSlider)
    {
        return;
    }

    isDraggingOutlineSlider_ = true;
    const float sliderValue = std::clamp((framebufferX - kOutlineSliderX) / kOutlineSliderWidth, 0.0f, 1.0f);
    setOutlineThickness(kOutlineMinThickness + sliderValue * (kOutlineMaxThickness - kOutlineMinThickness));
}

void Scene::setOutlineThickness(float thickness)
{
    outlineThickness_ = std::clamp(thickness, kOutlineMinThickness, kOutlineMaxThickness);
}
