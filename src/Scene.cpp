#define NOMINMAX

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "Scene.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr float kSandWorldYOffset = -0.95f;
    constexpr float kSandWorldScale = 3.0f;
    constexpr float kFallbackSandHeight = -1.0f;
    constexpr float kSquidwardQuestRadius = 1.35f;
    constexpr float kCollectibleJellyfishPickupRadius = 1.05f;
    constexpr float kPatrickInteractionRadius = 2.0f; 
    constexpr float kGaryInteractionRadius = 1.2f; 
    const glm::vec2 kGaryPosition(2.5f, -1.0f);
    const glm::vec2 kPatrickPosition(-5.0f, -3.0f);
    const glm::vec2 kSquidwardQuestPosition(2.5f, -5.0f);  // Po prawej od SpongeBoba
    const glm::vec3 kCollectibleJellyfishPositions[Scene::kCollectibleJellyfishCount] = {
        glm::vec3(24.6f, 0.12f,  5.4f),
        glm::vec3(25.2f, 0.18f,  6.7f),
        glm::vec3(24.7f, 0.10f,  8.0f),
        glm::vec3(25.3f, 0.16f,  9.3f),
        glm::vec3(24.8f, 0.14f, 10.6f)
    };

    int parseObjVertexIndex(const std::string& token)
    {
        const size_t slash = token.find('/');
        const std::string indexText = slash == std::string::npos ? token : token.substr(0, slash);
        return std::stoi(indexText) - 1;
    }
}

Scene::Scene(int width, int height)
    : width_(width),
      height_(height)
{
    loadSandCollisionMesh("assets/models/scene/sand.obj");
    collectibleJellyfishActive_.fill(false);
    audioEngine_ = new ma_engine();
    patrickDancing_ = false;

    if (ma_engine_init(NULL, audioEngine_) == MA_SUCCESS)
    {
        backgroundMusic_ = new ma_sound();
        if (ma_sound_init_from_file(audioEngine_, "assets/music.mp3", 0x00000003, NULL, NULL, backgroundMusic_) == MA_SUCCESS)
        {
            ma_sound_set_looping(backgroundMusic_, MA_TRUE);
            ma_sound_start(backgroundMusic_);
        }

        taskStartSound_ = new ma_sound();
        if (ma_sound_init_from_file(audioEngine_, "assets/voice_lines/spongebob-task-start.mp3", 0, NULL, NULL, taskStartSound_) != MA_SUCCESS)
        {
            delete taskStartSound_;
            taskStartSound_ = nullptr;
        }

        taskEndSound_ = new ma_sound();
        if (ma_sound_init_from_file(audioEngine_, "assets/voice_lines/task-end-sound.mp3", 0, NULL, NULL, taskEndSound_) != MA_SUCCESS)
        {
            delete taskEndSound_;
            taskEndSound_ = nullptr;
        }
        for (int i = 0; i < kGarySoundsCount; ++i)
        {
            garySounds_[i] = new ma_sound();
            std::string soundPath = "assets/voice_lines/garry_sounds_" + std::to_string(i + 1) + ".mp3";

            if (ma_sound_init_from_file(audioEngine_, soundPath.c_str(), 0, NULL, NULL, garySounds_[i]) != MA_SUCCESS)
            {
                delete garySounds_[i];
                garySounds_[i] = nullptr;
            }
        }
    }
}

void Scene::updateFramebufferSize(int width, int height)
{
    width_ = width;
    height_ = height;
}

void Scene::processInput(GLFWwindow* window)
{
    const bool isEscapePressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (isEscapePressed && !wasEscapePressed_)
    {
        menuOpen_ = !menuOpen_;
    }
    wasEscapePressed_ = isEscapePressed;

    if (menuOpen_)
    {
        characterMoving_ = false;
        wasSpacePressed_ = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        wasEnterPressed_ = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        return;
    }

    const float rotationVelocity = kCameraRotationSpeed * deltaTime_;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        characterYaw_ += rotationVelocity * 0.03f;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        characterYaw_ -= rotationVelocity * 0.03f;

    }
    glm::vec3 front;
    front.x = cos(characterYaw_);
    front.y = 0.0f;
    front.z = sin(characterYaw_);
    front = glm::normalize(front);

    const float velocity = kCameraSpeed * deltaTime_;
    const glm::vec3 previousPosition = characterPosition_;
    glm::vec3 candidatePosition = characterPosition_;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        candidatePosition += front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        candidatePosition -= front * velocity;
    }
    characterPosition_ = applyCharacterPhysics(candidatePosition);
    const glm::vec2 movement(characterPosition_.x - previousPosition.x, characterPosition_.z - previousPosition.z);
    characterMoving_ = glm::dot(movement, movement) > 0.000001f;
    const float zoomSpeed = 2.0f * deltaTime_;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        cameraDistance_ = std::max(1.0f, cameraDistance_ - zoomSpeed);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        cameraDistance_ = std::min(6.0f, cameraDistance_ + zoomSpeed);
    }

    const float cameraOrbitSpeed = kCameraRotationSpeed * deltaTime_ * 0.03f;
    const float cameraVerticalSpeed = 2.0f * deltaTime_;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        cameraYawOffset_ += cameraOrbitSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        cameraYawOffset_ -= cameraOrbitSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        cameraHeightAbove_ = std::min(3.0f, cameraHeightAbove_ + cameraVerticalSpeed);
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        cameraHeightAbove_ = std::max(1.0f, cameraHeightAbove_ - cameraVerticalSpeed);
    }

    const bool isSpacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (isSpacePressed && !wasSpacePressed_)
    {
        const glm::vec2 playerPosition(characterPosition_.x, characterPosition_.z);
        if (glm::distance(playerPosition, kPatrickPosition) <= kPatrickInteractionRadius)
        {
            patrickDancing_ = !patrickDancing_; 
        }
        else if (glm::distance(playerPosition, kGaryPosition) <= kGaryInteractionRadius)
        {
            int randomIndex = rand() % kGarySoundsCount; 

            if (garySounds_[randomIndex] != nullptr)
            {
                ma_sound_seek_to_pcm_frame(garySounds_[randomIndex], 0);
                ma_sound_start(garySounds_[randomIndex]);
            }
        }
        else if (!jellyfishQuestStarted_)
        {
            if (glm::distance(playerPosition, kSquidwardQuestPosition) <= kSquidwardQuestRadius)
            {
                jellyfishQuestStarted_ = true;
                jellyfishQuestIntroFinished_ = taskStartSound_ == nullptr;
                playerJellyFishCount_ = 0;
                collectibleJellyfishActive_.fill(jellyfishQuestIntroFinished_);
                if (taskStartSound_ != nullptr)
                {
                    ma_sound_seek_to_pcm_frame(taskStartSound_, 0);
                    ma_sound_start(taskStartSound_);
                }
            }
        }
        else if (jellyfishQuestIntroFinished_ && playerJellyFishCount_ < kCollectibleJellyfishCount)
        {
            for (int i = 0; i < kCollectibleJellyfishCount; ++i)
            {
                if (!collectibleJellyfishActive_[static_cast<std::size_t>(i)])
                {
                    continue;
                }

                const glm::vec3 jellyfishPosition = getCollectibleJellyfishPosition(i);
                const glm::vec2 jellyfishPosition2d(jellyfishPosition.x, jellyfishPosition.z);
                if (glm::distance(playerPosition, jellyfishPosition2d) <= kCollectibleJellyfishPickupRadius)
                {
                    collectibleJellyfishActive_[static_cast<std::size_t>(i)] = false;
                    ++playerJellyFishCount_;
                    break;
                }
            }
        }
    }
    wasSpacePressed_ = isSpacePressed;

    const bool isEnterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    if (jellyfishQuestStarted_ && !jellyfishQuestCompleted_ && playerJellyFishCount_ >= kCollectibleJellyfishCount)
    {
        const glm::vec2 playerPosition(characterPosition_.x, characterPosition_.z);
        if (glm::distance(playerPosition, kSquidwardQuestPosition) <= kSquidwardQuestRadius)
        {
            jellyfishQuestCompleted_ = true;
            if (taskEndSound_ != nullptr)
            {
                ma_sound_seek_to_pcm_frame(taskEndSound_, 0);
                ma_sound_start(taskEndSound_);
            }
        }
    }
    wasEnterPressed_ = isEnterPressed;

    updateCamera();
}

void Scene::updateDeltaTime(float currentFrameTime)
{
    deltaTime_ = currentFrameTime - lastFrameTime_;
    lastFrameTime_ = currentFrameTime;

    if (!menuOpen_)
    {
        const int visibleJellyfishCount = std::clamp(jellyfishCount_, kMinJellyfishCount, kMaxJellyfishCount);
        for (int i = 0; i < visibleJellyfishCount; ++i)
        {
            jellyfishAnimationTimes_[static_cast<std::size_t>(i)] += deltaTime_;
        }
    }
    if (jellyfishQuestStarted_ && !jellyfishQuestIntroFinished_ && taskStartSound_ != nullptr && ma_sound_at_end(taskStartSound_))
    {
        jellyfishQuestIntroFinished_ = true;
        collectibleJellyfishActive_.fill(true);
    }
    bubbleSpawnTimer_ += deltaTime_;
    if (bubbleSpawnTimer_ >= 0.3f)
    {
        bubbleSpawnTimer_ = 0.0f;
        if (bubbles_.size() < 40)
        {
            Bubble newBubble;

            // Random start position on the sand
            float randomX = -42.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 84.0f);
            float randomZ = -42.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 84.0f);
            float startY = getSandHeight(randomX, randomZ);

            glm::vec3 startPos(randomX, startY, randomZ);

            // Random end position high above
            float endY = startY + 8.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 3.0f);

            // Random bubble size
            float randomFraction = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            newBubble.size = 0.05f + randomFraction * (0.15f - 0.05f);

            // Random speed (how fast it moves along the curve)
            newBubble.speed = 0.08f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 0.12f);
            newBubble.pathProgress = 0.0f;

            // Choose random path type: 50% spiral, 50% Bezier curve
            bool useSpiral = (rand() % 2) == 0;

            if (useSpiral)
            {
                // Generate spiral path with PTF
                float radius = 0.3f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 0.5f);
                float turns = 1.5f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / 2.0f);
                newBubble.path = CurvePathGenerator::generateSpiralPathWithPTF(
                    startPos,
                    endY - startY,
                    radius,
                    turns,
                    60
                );
            }
            else
            {
                glm::vec3 endPos(
                    randomX + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 4.0f,
                    endY,
                    randomZ + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 4.0f
                );
                float height = endY - startY;
                glm::vec3 p1 = startPos + glm::vec3(
                    (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f,
                    height * 0.33f,
                    (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f
                );
                glm::vec3 p2 = startPos + glm::vec3(
                    (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f,
                    height * 0.67f,
                    (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f
                );

                newBubble.path = CurvePathGenerator::generateBezierPathWithPTF(
                    startPos,
                    p1,
                    p2,
                    endPos,
                    60
                );
            }
            if (!newBubble.path.empty())
            {
                newBubble.position = newBubble.path[0].position;
                newBubble.tangent = newBubble.path[0].tangent;
                newBubble.normal = newBubble.path[0].normal;
                newBubble.binormal = newBubble.path[0].binormal;
            }

            bubbles_.push_back(newBubble);
        }
    }
    for (auto it = bubbles_.begin(); it != bubbles_.end(); )
    {
        it->pathProgress += it->speed * deltaTime_;

        if (it->pathProgress >= 1.0f)
        {
            it = bubbles_.erase(it);
        }
        else
        {
            auto pathPoint = CurvePathGenerator::interpolatePath(it->path, it->pathProgress);
            it->position = pathPoint.position;
            it->tangent = pathPoint.tangent;
            it->normal = pathPoint.normal;
            it->binormal = pathPoint.binormal;
            ++it;
        }
    }
}

void Scene::renderUi(GLFWwindow* window)
{
    if (!menuOpen_)
    {
        const glm::vec2 playerPosition(characterPosition_.x, characterPosition_.z);
        const bool nearSquidward = glm::distance(playerPosition, kSquidwardQuestPosition) <= kSquidwardQuestRadius;

        if (jellyfishQuestStarted_ || nearSquidward)
        {
            ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f);
            const ImGuiWindowFlags questFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav;

            ImGui::Begin("QuestHud", nullptr, questFlags);
            if (!jellyfishQuestStarted_)
            {
                ImGui::TextUnformatted("Press SPACE to start the quest.");
            }
            else if (jellyfishQuestCompleted_)
            {
                ImGui::TextUnformatted("Task complete.");
            }
            else if (!jellyfishQuestIntroFinished_)
            {
                ImGui::TextUnformatted("Squidward is explaining the task...");
            }
            else if (playerJellyFishCount_ < kCollectibleJellyfishCount)
            {
                ImGui::Text("Quest: Bring 5 white jellyfish to Squidward. %d/%d", playerJellyFishCount_, kCollectibleJellyfishCount);
            }
            else
            {
                ImGui::TextUnformatted("Quest: Return to Squidward.");
            }
            ImGui::End();
        }
    }

    if (!menuOpen_)
    {
        return;
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin("Pause Menu", nullptr, flags);

    if (ImGui::Button("Resume", ImVec2(-1.0f, 0.0f)))
    {
        menuOpen_ = false;
    }

    ImGui::Separator();
    ImGui::Checkbox("Toon shading", &toonShadingEnabled_);
    ImGui::Text("Current shading: %s", toonShadingEnabled_ ? "Toon" : "PBR");
    ImGui::TextUnformatted("Corals: Toon");
    ImGui::SliderFloat("Outline", &outlineThickness_, kOutlineMinThickness, kOutlineMaxThickness, "%.3f");
    outlineThickness_ = std::clamp(outlineThickness_, kOutlineMinThickness, kOutlineMaxThickness);

    ImGui::Separator();
    if (ImGui::Button("Quit", ImVec2(-1.0f, 0.0f)))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    ImGui::End();
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

float Scene::getElapsedTime() const
{
    return lastFrameTime_;
}

float Scene::getOutlineThickness() const
{
    return outlineThickness_;
}

int Scene::getJellyfishCount() const
{
    return jellyfishCount_;
}

float Scene::getJellyfishAnimationTime(int index) const
{
    if (index < 0 || index >= kMaxJellyfishCount)
    {
        return 0.0f;
    }

    return jellyfishAnimationTimes_[static_cast<std::size_t>(index)];
}

int Scene::getPlayerJellyFishCount() const
{
    return playerJellyFishCount_;
}

bool Scene::isCollectibleJellyfishActive(int index) const
{
    if (index < 0 || index >= kCollectibleJellyfishCount)
    {
        return false;
    }

    return collectibleJellyfishActive_[static_cast<std::size_t>(index)];
}

glm::vec3 Scene::getCollectibleJellyfishPosition(int index) const
{
    if (index < 0 || index >= kCollectibleJellyfishCount)
    {
        return glm::vec3(0.0f);
    }

    return kCollectibleJellyfishPositions[index];
}

bool Scene::isToonShadingEnabled() const
{
    return toonShadingEnabled_;
}

bool Scene::isMenuOpen() const
{
    return menuOpen_;
}

void Scene::clearCollisionBoxes()
{
    collisionBoxes_.clear();
    collisionEllipses_.clear();
}

void Scene::addCollisionBox(const glm::vec2& minBounds, const glm::vec2& maxBounds)
{
    collisionBoxes_.push_back({minBounds, maxBounds});
}

void Scene::addCollisionEllipse(const glm::vec2& center, const glm::vec2& radii)
{
    collisionEllipses_.push_back({center, radii});
}

void Scene::updateCamera()
{
    const float totalCameraYaw = characterYaw_ + cameraYawOffset_;

    glm::vec3 cameraFrontVec;
    cameraFrontVec.x = cos(totalCameraYaw);
    cameraFrontVec.y = 0.0f;
    cameraFrontVec.z = sin(totalCameraYaw);
    cameraFrontVec = glm::normalize(cameraFrontVec);

    const glm::vec3 cameraPos = characterPosition_ - (cameraFrontVec * cameraDistance_) + glm::vec3(0.0f, cameraHeightAbove_, 0.0f);
    camera_.setPosition(cameraPos);

    const glm::quat targetOrientation = glm::angleAxis(-totalCameraYaw - glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const float pitchDeg = -4.0f - (cameraHeightAbove_ * 10.0f);
    const glm::quat pitchAngle = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(1.0f, 0.0f, 0.0f));
    camera_.setOrientation(targetOrientation * pitchAngle);
}

glm::vec3 Scene::applyCharacterPhysics(const glm::vec3& candidatePosition) const
{
    glm::vec3 resolved = candidatePosition;

    constexpr float sandHalfExtent = 14.0f * kSandWorldScale;
    resolved.x = std::clamp(resolved.x, -sandHalfExtent, sandHalfExtent);
    resolved.z = std::clamp(resolved.z, -sandHalfExtent, sandHalfExtent);
    resolved.y = getSandHeight(resolved.x, resolved.z);

    resolved = resolveSceneCollisions(resolved);
    resolved.y = getSandHeight(resolved.x, resolved.z);

    return resolved;
}

glm::vec3 Scene::resolveSceneCollisions(const glm::vec3& position) const
{
    constexpr float characterRadius = 0.18f;

    glm::vec2 resolved(position.x, position.z);
    for (const CollisionBox& collider : collisionBoxes_)
    {
        const glm::vec2 closestPoint(
            std::clamp(resolved.x, collider.minBounds.x, collider.maxBounds.x),
            std::clamp(resolved.y, collider.minBounds.y, collider.maxBounds.y)
        );
        glm::vec2 offset = resolved - closestPoint;
        float distanceSquared = glm::dot(offset, offset);

        if (distanceSquared > 0.0001f)
        {
            if (distanceSquared < characterRadius * characterRadius)
            {
                const float distance = std::sqrt(distanceSquared);
                resolved = closestPoint + offset / distance * characterRadius;
            }
            continue;
        }

        const float pushLeft = std::abs(resolved.x - collider.minBounds.x);
        const float pushRight = std::abs(collider.maxBounds.x - resolved.x);
        const float pushBack = std::abs(resolved.y - collider.minBounds.y);
        const float pushFront = std::abs(collider.maxBounds.y - resolved.y);
        const float minPush = std::min({pushLeft, pushRight, pushBack, pushFront});

        if (minPush == pushLeft)
        {
            resolved.x = collider.minBounds.x - characterRadius;
        }
        else if (minPush == pushRight)
        {
            resolved.x = collider.maxBounds.x + characterRadius;
        }
        else if (minPush == pushBack)
        {
            resolved.y = collider.minBounds.y - characterRadius;
        }
        else
        {
            resolved.y = collider.maxBounds.y + characterRadius;
        }
    }

    for (const CollisionEllipse& collider : collisionEllipses_)
    {
        const glm::vec2 inflatedRadii = collider.radii + glm::vec2(characterRadius);
        if (inflatedRadii.x <= 0.0f || inflatedRadii.y <= 0.0f)
        {
            continue;
        }

        glm::vec2 offset = resolved - collider.center;
        const float normalizedDistance =
            (offset.x * offset.x) / (inflatedRadii.x * inflatedRadii.x) +
            (offset.y * offset.y) / (inflatedRadii.y * inflatedRadii.y);

        if (normalizedDistance >= 1.0f)
        {
            continue;
        }

        if (glm::dot(offset, offset) < 0.0001f)
        {
            resolved = collider.center + glm::vec2(inflatedRadii.x, 0.0f);
            continue;
        }

        resolved = collider.center + offset / std::sqrt(normalizedDistance);
    }

    return glm::vec3(resolved.x, position.y, resolved.y);
}

float Scene::getSandHeight(float x, float z) const
{
    constexpr float epsilon = 0.0001f;
    const glm::vec2 point(x, z);

    for (const SandTriangle& triangle : sandTriangles_)
    {
        const glm::vec2 a(triangle.a.x, triangle.a.z);
        const glm::vec2 b(triangle.b.x, triangle.b.z);
        const glm::vec2 c(triangle.c.x, triangle.c.z);

        const float minX = std::min({a.x, b.x, c.x}) - epsilon;
        const float maxX = std::max({a.x, b.x, c.x}) + epsilon;
        const float minZ = std::min({a.y, b.y, c.y}) - epsilon;
        const float maxZ = std::max({a.y, b.y, c.y}) + epsilon;
        if (point.x < minX || point.x > maxX || point.y < minZ || point.y > maxZ)
        {
            continue;
        }

        const glm::vec2 v0 = b - a;
        const glm::vec2 v1 = c - a;
        const glm::vec2 v2 = point - a;
        const float denominator = v0.x * v1.y - v1.x * v0.y;
        if (std::abs(denominator) < epsilon)
        {
            continue;
        }

        const float u = (v2.x * v1.y - v1.x * v2.y) / denominator;
        const float v = (v0.x * v2.y - v2.x * v0.y) / denominator;
        if (u >= -epsilon && v >= -epsilon && u + v <= 1.0f + epsilon)
        {
            return triangle.a.y + u * (triangle.b.y - triangle.a.y) + v * (triangle.c.y - triangle.a.y) + kSandWorldYOffset;
        }
    }

    return kFallbackSandHeight;
}

void Scene::loadSandCollisionMesh(const char* path)
{
    std::ifstream file(path);
    if (!file)
    {
        return;
    }

    std::vector<glm::vec3> vertices;
    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream stream(line);
        std::string command;
        stream >> command;

        if (command == "v")
        {
            glm::vec3 vertex(0.0f);
            stream >> vertex.x >> vertex.y >> vertex.z;
            vertex.x *= kSandWorldScale;
            vertex.z *= kSandWorldScale;
            vertices.push_back(vertex);
        }
        else if (command == "f")
        {
            std::vector<int> face;
            std::string token;
            while (stream >> token)
            {
                face.push_back(parseObjVertexIndex(token));
            }

            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                const int indices[] = {face[0], face[i], face[i + 1]};
                if (indices[0] < 0 || indices[1] < 0 || indices[2] < 0 ||
                    static_cast<size_t>(indices[0]) >= vertices.size() ||
                    static_cast<size_t>(indices[1]) >= vertices.size() ||
                    static_cast<size_t>(indices[2]) >= vertices.size())
                {
                    continue;
                }

                sandTriangles_.push_back({
                    vertices[static_cast<size_t>(indices[0])],
                    vertices[static_cast<size_t>(indices[1])],
                    vertices[static_cast<size_t>(indices[2])]
                });
            }
        }
    }
}
void Scene::handleMouseMovement(double xpos, double ypos)
{
    if (menuOpen_) return;

    static float lastX = static_cast<float>(width_) * 0.5f;
    static float lastY = static_cast<float>(height_) * 0.5f;
    static bool firstMouse = true;

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    constexpr float sensitivity = 0.005f;
    xoffset *= sensitivity;
    yoffset *= sensitivity * 2.0f;

    cameraYawOffset_ -= xoffset;

    cameraHeightAbove_ = std::clamp(cameraHeightAbove_ + yoffset, -5.0f, 10.0f);
}

Scene::~Scene()
{
    if (taskStartSound_)
    {
        ma_sound_uninit(taskStartSound_);
        delete taskStartSound_;
        taskStartSound_ = nullptr;
    }
    if (taskEndSound_)
    {
        ma_sound_uninit(taskEndSound_);
        delete taskEndSound_;
        taskEndSound_ = nullptr;
    }
    for (int i = 0; i < kGarySoundsCount; ++i)
    {
        if (garySounds_[i])
        {
            ma_sound_uninit(garySounds_[i]);
            delete garySounds_[i];
            garySounds_[i] = nullptr;
        }
    }
    if (backgroundMusic_)
    {
        ma_sound_uninit(backgroundMusic_);
        delete backgroundMusic_;
        backgroundMusic_ = nullptr;
    }
    if (audioEngine_)
    {
        ma_engine_uninit(audioEngine_);
        delete audioEngine_;
        audioEngine_ = nullptr;
    }
    
}
