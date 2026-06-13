#pragma once

#include "Camera.h"

struct GLFWwindow;

class Scene
{
public:
    static constexpr float kOutlineMinThickness = 0.01f;
    static constexpr float kOutlineMaxThickness = 0.12f;
    static constexpr float kOutlineSliderX = 24.0f;
    static constexpr float kOutlineSliderY = 24.0f;
    static constexpr float kOutlineSliderWidth = 220.0f;
    static constexpr float kOutlineSliderHeight = 18.0f;
    static constexpr float kToonToggleX = 24.0f;
    static constexpr float kToonToggleY = 58.0f;
    static constexpr float kToonToggleWidth = 118.0f;
    static constexpr float kToonToggleHeight = 28.0f;

    Scene(int width, int height);

    void updateFramebufferSize(int width, int height);
    void processInput(GLFWwindow* window);
    void updateDeltaTime(float currentFrameTime);

    Camera& getCamera();
    const Camera& getCamera() const;
    float getAspectRatio() const;
    float getFramebufferWidth() const;
    float getFramebufferHeight() const;
    float getOutlineThickness() const;
    float getOutlineSliderValue() const;
    bool isToonShadingEnabled() const;

    glm::vec3 getCharacterPosition() const { return characterPosition_; }
    float getCharacterYaw() const { return characterYaw_; }

private:
    static constexpr float kCameraSpeed = 2.5f;
    static constexpr float kCameraRotationSpeed = 90.0f;

    void updateUi(GLFWwindow* window);
    void setOutlineThickness(float thickness);

    Camera camera_;
    int width_;
    int height_;
    float lastFrameTime_ = 0.0f;
    float deltaTime_ = 0.0f;
    float outlineThickness_ = 0.05f;
    float cameraYawOffset_ = 0.0f; // Dodatkowy obrót kamery wokół postaci
    float cameraHeightAbove_ = 1.0f;


    bool toonShadingEnabled_ = true;
    bool isDraggingOutlineSlider_ = false;
    bool wasLeftMousePressed_ = false;

    glm::vec3 characterPosition_ = glm::vec3(0.0f, -1.0f, -1.0f); 
    float characterYaw_ = -90.0f;
    float cameraDistance_ = 2.5f;
};
