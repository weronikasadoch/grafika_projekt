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
}

void Scene::renderUi(GLFWwindow* window)
{
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
    ImGui::TextUnformatted("Corals: PBR");
    ImGui::SliderFloat("Outline", &outlineThickness_, kOutlineMinThickness, kOutlineMaxThickness, "%.3f");
    outlineThickness_ = std::clamp(outlineThickness_, kOutlineMinThickness, kOutlineMaxThickness);
    ImGui::SliderInt("Jellyfish", &jellyfishCount_, kMinJellyfishCount, kMaxJellyfishCount);
    jellyfishCount_ = std::clamp(jellyfishCount_, kMinJellyfishCount, kMaxJellyfishCount);

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
