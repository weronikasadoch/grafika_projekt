#pragma once

#include "Camera.h"

#include <array>
#include <vector>

struct GLFWwindow;

class Scene
{
public:
    static constexpr float kOutlineMinThickness = 0.01f;
    static constexpr float kOutlineMaxThickness = 0.12f;
    static constexpr int kMinJellyfishCount = 0;
    static constexpr int kMaxJellyfishCount = 10;

    Scene(int width, int height);

    void updateFramebufferSize(int width, int height);
    void processInput(GLFWwindow* window);
    void updateDeltaTime(float currentFrameTime);
    void renderUi(GLFWwindow* window);

    Camera& getCamera();
    const Camera& getCamera() const;
    float getAspectRatio() const;
    float getFramebufferWidth() const;
    float getFramebufferHeight() const;
    float getElapsedTime() const;
    float getOutlineThickness() const;
    int getJellyfishCount() const;
    float getJellyfishAnimationTime(int index) const;
    bool isToonShadingEnabled() const;
    bool isMenuOpen() const;

    glm::vec3 getCharacterPosition() const { return characterPosition_; }
    float getCharacterYaw() const { return characterYaw_; }

private:
    static constexpr float kCameraSpeed = 2.5f;
    static constexpr float kCameraRotationSpeed = 90.0f;

    void updateCamera();
    glm::vec3 applyCharacterPhysics(const glm::vec3& candidatePosition) const;
    bool collidesWithHouse(const glm::vec3& position) const;
    float getSandHeight(float x, float z) const;
    void loadSandCollisionMesh(const char* path);

    struct SandTriangle
    {
        glm::vec3 a;
        glm::vec3 b;
        glm::vec3 c;
    };

    Camera camera_;
    std::vector<SandTriangle> sandTriangles_;
    int width_;
    int height_;
    float lastFrameTime_ = 0.0f;
    float deltaTime_ = 0.0f;
    float outlineThickness_ = 0.05f;
    float cameraYawOffset_ = 0.0f; // Dodatkowy obrót kamery wokół postaci
    float cameraHeightAbove_ = 1.0f;


    bool toonShadingEnabled_ = true;
    bool wasEscapePressed_ = false;
    bool menuOpen_ = false;
    int jellyfishCount_ = kMaxJellyfishCount;
    std::array<float, kMaxJellyfishCount> jellyfishAnimationTimes_ = {};

    glm::vec3 characterPosition_ = glm::vec3(0.0f, -1.0f, -1.0f); 
    float characterYaw_ = -90.0f;
    float cameraDistance_ = 2.5f;
};
