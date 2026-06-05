#include "Renderer.h"

#include "Scene.h"
#include "Shader_Loader.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
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
    char toonVertexShaderPath[] = "shaders/toon.vert";
    char toonFragmentShaderPath[] = "shaders/toon.frag";
    char outlineVertexShaderPath[] = "shaders/outline.vert";
    char outlineFragmentShaderPath[] = "shaders/outline.frag";
    char uiVertexShaderPath[] = "shaders/ui.vert";
    char uiFragmentShaderPath[] = "shaders/ui.frag";
    toonProgram_ = shaderLoader.CreateProgram(toonVertexShaderPath, toonFragmentShaderPath);
    outlineProgram_ = shaderLoader.CreateProgram(outlineVertexShaderPath, outlineFragmentShaderPath);
    uiProgram_ = shaderLoader.CreateProgram(uiVertexShaderPath, uiFragmentShaderPath);
    sphere_ = createSphereMesh(1.0f, 48, 24);
    createUiResources();

    return toonProgram_ != 0 && outlineProgram_ != 0 && uiProgram_ != 0 && sphere_.vao != 0 && uiVao_ != 0;
}

void Renderer::render(const Scene& scene)
{
    glClearColor(0.10f, 0.72f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());

    const glm::mat4 leftSphere = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0.0f, 0.0f));
    const glm::mat4 rightSphere = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(1.35f, 0.15f, -0.35f)),
        glm::vec3(0.75f)
    );

    renderSphere(leftSphere, view, projection, glm::vec3(1.0f, 0.82f, 0.24f), scene.getOutlineThickness());
    renderSphere(rightSphere, view, projection, glm::vec3(0.28f, 0.72f, 1.0f), scene.getOutlineThickness());
    renderOutlineSlider(scene);
}

void Renderer::shutdown()
{
    deleteUiResources();
    deleteMesh(sphere_);
    glDeleteProgram(uiProgram_);
    glDeleteProgram(outlineProgram_);
    glDeleteProgram(toonProgram_);
    sphere_ = {};
    uiProgram_ = 0;
    outlineProgram_ = 0;
    toonProgram_ = 0;
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

void Renderer::createUiResources()
{
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);

    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::deleteMesh(const Mesh& mesh) const
{
    glDeleteBuffers(1, &mesh.ebo);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteVertexArrays(1, &mesh.vao);
}

void Renderer::deleteUiResources()
{
    glDeleteBuffers(1, &uiVbo_);
    glDeleteVertexArrays(1, &uiVao_);
}

void Renderer::renderSphere(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& baseColor, float outlineThickness) const
{
    glEnable(GL_CULL_FACE);

    glCullFace(GL_FRONT);
    glUseProgram(outlineProgram_);
    setMat4(outlineProgram_, "uModel", model);
    setMat4(outlineProgram_, "uView", view);
    setMat4(outlineProgram_, "uProjection", projection);
    setFloat(outlineProgram_, "uOutlineThickness", outlineThickness);
    setVec3(outlineProgram_, "uOutlineColor", glm::vec3(0.0f, 0.04f, 0.22f));
    drawSphere();

    glCullFace(GL_BACK);
    glUseProgram(toonProgram_);
    setMat4(toonProgram_, "uModel", model);
    setMat4(toonProgram_, "uView", view);
    setMat4(toonProgram_, "uProjection", projection);
    setVec3(toonProgram_, "uBaseColor", baseColor);
    setVec3(toonProgram_, "uLightDirection", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    setVec3(toonProgram_, "uAmbientColor", glm::vec3(0.08f, 0.18f, 0.22f));
    drawSphere();

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

void Renderer::renderOutlineSlider(const Scene& scene) const
{
    const float x = Scene::kOutlineSliderX;
    const float y = Scene::kOutlineSliderY;
    const float width = Scene::kOutlineSliderWidth;
    const float height = Scene::kOutlineSliderHeight;
    const float value = scene.getOutlineSliderValue();
    const float thumbWidth = 10.0f;
    const float thumbX = x + value * width - thumbWidth * 0.5f;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(uiProgram_);
    glUniform2f(glGetUniformLocation(uiProgram_, "uScreenSize"), scene.getFramebufferWidth(), scene.getFramebufferHeight());

    drawUiQuad(x - 4.0f, y - 4.0f, width + 8.0f, height + 8.0f, glm::vec3(0.03f, 0.11f, 0.18f));
    drawUiQuad(x, y, width, height, glm::vec3(0.06f, 0.19f, 0.31f));
    drawUiQuad(x, y, width * value, height, glm::vec3(0.0f, 0.10f, 0.38f));
    drawUiQuad(thumbX, y - 5.0f, thumbWidth, height + 10.0f, glm::vec3(0.72f, 0.88f, 1.0f));

    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawSphere() const
{
    glBindVertexArray(sphere_.vao);
    glDrawElements(GL_TRIANGLES, sphere_.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::drawUiQuad(float x, float y, float width, float height, const glm::vec3& color) const
{
    const float vertices[] = {
        x, y,
        x + width, y,
        x + width, y + height,
        x, y,
        x + width, y + height,
        x, y + height
    };

    setVec3(uiProgram_, "uColor", color);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::setMat4(GLuint program, const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Renderer::setVec3(GLuint program, const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(program, name), 1, glm::value_ptr(value));
}

void Renderer::setFloat(GLuint program, const char* name, float value) const
{
    glUniform1f(glGetUniformLocation(program, name), value);
}
