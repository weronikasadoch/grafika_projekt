#include "Renderer.h"

#include "Scene.h"
#include "Shader_Loader.h"

#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#include <cmath>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
}

bool Renderer::initialize()
{
    glEnable(GL_DEPTH_TEST);

    Core::Shader_Loader shaderLoader;
    char vertexShaderPath[] = "shaders/toon.vert";
    char fragmentShaderPath[] = "shaders/toon.frag";
    shaderProgram_ = shaderLoader.CreateProgram(vertexShaderPath, fragmentShaderPath);
    sphere_ = createSphereMesh(1.0f, 48, 24);

    return shaderProgram_ != 0 && sphere_.vao != 0;
}

void Renderer::render(const Scene& scene)
{
    glClearColor(0.10f, 0.72f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 model = glm::mat4(1.0f);
    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());

    glUseProgram(shaderProgram_);
    setMat4("uModel", model);
    setMat4("uView", view);
    setMat4("uProjection", projection);
    setVec3("uBaseColor", glm::vec3(1.0f, 0.82f, 0.24f));
    setVec3("uLightDirection", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    setVec3("uAmbientColor", glm::vec3(0.08f, 0.18f, 0.22f));

    glBindVertexArray(sphere_.vao);
    glDrawElements(GL_TRIANGLES, sphere_.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::shutdown()
{
    deleteMesh(sphere_);
    glDeleteProgram(shaderProgram_);
    sphere_ = {};
    shaderProgram_ = 0;
}

Renderer::Mesh Renderer::createSphereMesh(float radius, int sectors, int stacks) const
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int stack = 0; stack <= stacks; ++stack)
    {
        const float stackAngle = kPi / 2.0f - static_cast<float>(stack) * kPi / static_cast<float>(stacks);
        const float xy = radius * std::cos(stackAngle);
        const float z = radius * std::sin(stackAngle);

        for (int sector = 0; sector <= sectors; ++sector)
        {
            const float sectorAngle = static_cast<float>(sector) * 2.0f * kPi / static_cast<float>(sectors);
            const float x = xy * std::cos(sectorAngle);
            const float y = xy * std::sin(sectorAngle);
            const glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    for (int stack = 0; stack < stacks; ++stack)
    {
        int current = stack * (sectors + 1);
        int next = current + sectors + 1;

        for (int sector = 0; sector < sectors; ++sector, ++current, ++next)
        {
            if (stack != 0)
            {
                indices.push_back(static_cast<unsigned int>(current));
                indices.push_back(static_cast<unsigned int>(next));
                indices.push_back(static_cast<unsigned int>(current + 1));
            }

            if (stack != stacks - 1)
            {
                indices.push_back(static_cast<unsigned int>(current + 1));
                indices.push_back(static_cast<unsigned int>(next));
                indices.push_back(static_cast<unsigned int>(next + 1));
            }
        }
    }

    Mesh mesh;
    mesh.indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return mesh;
}

void Renderer::deleteMesh(const Mesh& mesh) const
{
    glDeleteBuffers(1, &mesh.ebo);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteVertexArrays(1, &mesh.vao);
}

void Renderer::setMat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Renderer::setVec3(const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(shaderProgram_, name), 1, glm::value_ptr(value));
}
