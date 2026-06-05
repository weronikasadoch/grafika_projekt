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

    Mesh createSphereMesh(float radius, int sectors, int stacks) const;
    void deleteMesh(const Mesh& mesh) const;
    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;

    GLuint shaderProgram_ = 0;
    Mesh sphere_;
};
