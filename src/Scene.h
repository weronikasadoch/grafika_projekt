#pragma once

#include "Camera.h"
#include <glm.hpp>
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
    static constexpr int kCollectibleJellyfishCount = 5;

    Scene(int width, int height);
    ~Scene();
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
    int getPlayerJellyFishCount() const;
    bool isJellyfishQuestStarted() const { return jellyfishQuestStarted_; }
    bool isJellyfishQuestComplete() const { return jellyfishQuestStarted_ && playerJellyFishCount_ >= kCollectibleJellyfishCount; }
    bool isCollectibleJellyfishActive(int index) const;
    glm::vec3 getCollectibleJellyfishPosition(int index) const;
    bool isToonShadingEnabled() const;
    bool isMenuOpen() const;
    float getSandHeight(float x, float z) const;
    void clearCollisionBoxes();
    void addCollisionBox(const glm::vec2& minBounds, const glm::vec2& maxBounds);
    void addCollisionEllipse(const glm::vec2& center, const glm::vec2& radii);

    glm::vec3 getCharacterPosition() const { return characterPosition_; }
    float getCharacterYaw() const { return characterYaw_; }
    bool isCharacterMoving() const { return characterMoving_; }
    void handleMouseMovement(double xpos, double ypos);
    struct Bubble
    {
        glm::vec3 position;
        float speed;
        float size;
        float wobbleSpeed;
        float wobbleTime;
    };
    const std::vector<Bubble>& getBubbles() const { return bubbles_; }
    

private:
    static constexpr float kCameraSpeed = 2.5f;
    static constexpr float kCameraRotationSpeed = 90.0f;

    void updateCamera();
    glm::vec3 applyCharacterPhysics(const glm::vec3& candidatePosition) const;
    glm::vec3 resolveSceneCollisions(const glm::vec3& position) const;
    void loadSandCollisionMesh(const char* path);

    struct SandTriangle
    {
        glm::vec3 a;
        glm::vec3 b;
        glm::vec3 c;
    };

    struct CollisionBox
    {
        glm::vec2 minBounds;
        glm::vec2 maxBounds;
    };

    struct CollisionEllipse
    {
        glm::vec2 center;
        glm::vec2 radii;
    };

    Camera camera_;
    std::vector<SandTriangle> sandTriangles_;
    std::vector<CollisionBox> collisionBoxes_;
    std::vector<CollisionEllipse> collisionEllipses_;
    int width_;
    int height_;
    float lastFrameTime_ = 0.0f;
    float deltaTime_ = 0.0f;
    float outlineThickness_ = 0.05f;
    float cameraYawOffset_ = 0.0f; 
    float cameraHeightAbove_ = 1.0f;


    bool toonShadingEnabled_ = true;
    bool wasEscapePressed_ = false;
    bool wasSpacePressed_ = false;
    bool wasEnterPressed_ = false;
    bool menuOpen_ = false;
    bool jellyfishQuestStarted_ = false;
    bool jellyfishQuestIntroFinished_ = false;
    bool jellyfishQuestCompleted_ = false;
    int jellyfishCount_ = kMaxJellyfishCount;
    int playerJellyFishCount_ = 0;
    std::array<float, kMaxJellyfishCount> jellyfishAnimationTimes_ = {};
    std::array<bool, kCollectibleJellyfishCount> collectibleJellyfishActive_ = {};

    glm::vec3 characterPosition_ = glm::vec3(0.0f, -1.0f, -1.0f); 
    float characterYaw_ = -90.0f;
    bool characterMoving_ = false;
    float cameraDistance_ = 2.5f;
    std::vector<Bubble> bubbles_;
    float bubbleSpawnTimer_ = 0.0f;
    struct ma_engine* audioEngine_ = nullptr;
    struct ma_sound* backgroundMusic_ = nullptr;
    struct ma_sound* taskStartSound_ = nullptr;
};
