#pragma once

#include <GL/glew.h>
#include <glm.hpp>

#include <string>
#include <vector>

class AnimatedModel
{
public:
    bool loadFromGlb(const std::string& path);
    bool setActiveAnimation(const std::string& animationName);
    void updateAnimation(float elapsedTime);
    void draw() const;
    void destroy();

    bool isLoaded() const { return vao_ != 0 && indexCount_ > 0; }
    bool hasEmbeddedBaseColorTexture() const;
    const std::vector<unsigned char>& embeddedBaseColorTexture() const;
    glm::vec3 minBounds() const { return minBounds_; }
    glm::vec3 maxBounds() const { return maxBounds_; }
    float minY() const { return minBounds_.y; }
    float maxY() const { return maxBounds_.y; }

private:
    struct Impl;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    glm::vec3 minBounds_ = glm::vec3(0.0f);
    glm::vec3 maxBounds_ = glm::vec3(0.0f);
    std::vector<float> gpuVertices_;
    Impl* impl_ = nullptr;
};
