#pragma once

#include <GL/glew.h>

#include <string>

class Texture
{
public:
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

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;

};
