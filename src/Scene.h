#pragma once

#include "Camera.h"

struct GLFWwindow;

class Scene
{
public:
    Scene(int width, int height);

    void updateFramebufferSize(int width, int height);
    void processInput(GLFWwindow* window);
    void updateDeltaTime(float currentFrameTime);

    Camera& getCamera();
    const Camera& getCamera() const;
    float getAspectRatio() const;

private:
    static constexpr float kCameraSpeed = 2.5f;
    static constexpr float kCameraRotationSpeed = 90.0f;

    Camera camera_;
    int width_;
    int height_;
    float lastFrameTime_ = 0.0f;
    float deltaTime_ = 0.0f;
};
