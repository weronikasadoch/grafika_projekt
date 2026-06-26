#pragma once

#include <GL/glew.h>
#include <glm.hpp>

#include <string>
#include <vector>

class Texture
{
public:
    bool loadImage(const std::string& path);
    bool loadImageData(const unsigned char* data, int size);
    bool loadPPM(const std::string& path);
    void createSolidColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    void bind(GLenum textureUnit) const;
    void destroy();
    GLuint id() const { return texture_; }

private:
    GLuint texture_ = 0;
};

class Model
{
public:
    bool loadFromObj(const std::string& path);
    void draw() const;
    void drawInstanced(GLuint instanceBuffer, GLsizei instanceCount) const;
    void destroy();
    bool isLoaded() const { return vao_ != 0 && indexCount_ > 0; }
    glm::vec3 minBounds() const { return minBounds_; }
    glm::vec3 maxBounds() const { return maxBounds_; }
    float minY() const { return minBounds_.y; }
    float maxY() const { return maxBounds_.y; }
    const Texture* diffuseTexture() const { return hasDiffuseTexture_ ? &diffuseTexture_ : nullptr; }

private:
    struct DrawRange
    {
        GLsizei indexOffset = 0;
        GLsizei indexCount = 0;
        GLuint texture = 0;
        bool hasTexture = false;
    };

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    glm::vec3 minBounds_ = glm::vec3(0.0f);
    glm::vec3 maxBounds_ = glm::vec3(0.0f);
    Texture diffuseTexture_;
    bool hasDiffuseTexture_ = false;
    std::vector<GLuint> materialTextures_;
    std::vector<DrawRange> drawRanges_;
    GLuint whiteTexture_ = 0;

};
