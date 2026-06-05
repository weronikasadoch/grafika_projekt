#pragma once

#include <GL/glew.h>
#include <glm.hpp>

class Scene;

class Renderer
{
public:
    bool initialize();
    void render(const Scene& scene);
    void shutdown();

private:
    struct Mesh
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
    };

    void renderSphere(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& baseColor, float outlineThickness) const;
    void renderOutlineSlider(const Scene& scene) const;
    Mesh createSphereMesh(float radius, int sectors, int stacks) const;
    void createUiResources();
    void deleteMesh(const Mesh& mesh) const;
    void deleteUiResources();
    void drawSphere() const;
    void drawUiQuad(float x, float y, float width, float height, const glm::vec3& color) const;
    void setMat4(GLuint program, const char* name, const glm::mat4& value) const;
    void setVec3(GLuint program, const char* name, const glm::vec3& value) const;
    void setFloat(GLuint program, const char* name, float value) const;

    GLuint toonProgram_ = 0;
    GLuint outlineProgram_ = 0;
    GLuint uiProgram_ = 0;
    GLuint uiVao_ = 0;
    GLuint uiVbo_ = 0;
    Mesh sphere_;
};
