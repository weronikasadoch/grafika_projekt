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
    constexpr int kShadowMapSize = 2048;
    const glm::vec3 kLightDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
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
    char shadowVertexShaderPath[] = "shaders/shadow_depth.vert";
    char shadowFragmentShaderPath[] = "shaders/shadow_depth.frag";
    toonProgram_ = shaderLoader.CreateProgram(toonVertexShaderPath, toonFragmentShaderPath);
    outlineProgram_ = shaderLoader.CreateProgram(outlineVertexShaderPath, outlineFragmentShaderPath);
    uiProgram_ = shaderLoader.CreateProgram(uiVertexShaderPath, uiFragmentShaderPath);
    shadowProgram_ = shaderLoader.CreateProgram(shadowVertexShaderPath, shadowFragmentShaderPath);
    sphere_ = createSphereMesh(1.0f, 48, 24);
    const bool sandLoaded = sandModel_.loadFromObj("assets/models/scene/sand.obj");
    const bool spongebobLoaded = spongebobModel_.loadFromObj("assets/models/houses/spongebob/spongebob_house_1.obj");
    const bool patrickLoaded = patrickModel_.loadFromObj("assets/models/houses/patrick/patrick_house_1.obj");
    const bool squidwardLoaded = squidwardModel_.loadFromObj("assets/models/houses/squidward/squidward_house_1.obj");
    const bool characterLoaded = characterModel_.loadFromObj("assets/models/Spongebob_model/spongebob_model.obj");
    spongebobFallbackTexture_.createSolidColor(255, 214, 54);
    createUiResources();
    const bool shadowResourcesCreated = createShadowResources();

    return toonProgram_ != 0 &&
        outlineProgram_ != 0 &&
        uiProgram_ != 0 &&
        shadowProgram_ != 0 &&
        sphere_.vao != 0 &&
        sandLoaded &&
        spongebobLoaded &&
        patrickLoaded &&
        squidwardLoaded &&
        characterLoaded &&
        uiVao_ != 0 &&
        shadowResourcesCreated;
}

void Renderer::render(const Scene& scene)
{
    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());
    const glm::mat4 lightSpace = createLightSpaceMatrix();

    const glm::mat4 sand = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.95f, 0.0f));
    const glm::mat4 spongebobModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.90f, -3.0f)),
        glm::vec3(0.45f)
    );
    const glm::mat4 patrickModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, -0.90f, -2.6f)),
        glm::vec3(0.45f)
    );
    const glm::mat4 squidwardModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, -1.20f, -2.6f)),
        glm::vec3(0.45f)
    );
    glm::vec3 charPos = scene.getCharacterPosition();
    float charYaw = scene.getCharacterYaw();

    glm::mat4 characterModel = glm::mat4(1.0f);
    characterModel = glm::translate(characterModel, charPos); // <-- Tutaj aplikujemy ruch!
    //characterModel = glm::rotate(characterModel, -charYaw - glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Obrót
    characterModel = glm::rotate(characterModel, -charYaw + glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    characterModel = glm::scale(characterModel, glm::vec3(0.45f));

    //const glm::mat4 characterModel = glm::scale(
      //  glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -1.0f)),
        //glm::vec3(0.45f)
    //);

    renderShadowMap(lightSpace, spongebobModel, patrickModel, squidwardModel, characterModel);
    

    glViewport(0, 0, static_cast<GLsizei>(scene.getFramebufferWidth()), static_cast<GLsizei>(scene.getFramebufferHeight()));
    glClearColor(0.10f, 0.72f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderModel(sandModel_, sand, view, projection, lightSpace, glm::vec3(0.86f, 0.68f, 0.38f), 0.0f, 1.65f, true, scene.isToonShadingEnabled());
    renderModel(spongebobModel_, spongebobModel, view, projection, lightSpace, glm::vec3(1.0f, 0.72f, 0.20f), scene.getOutlineThickness(), 2.6f, true, scene.isToonShadingEnabled());
    renderModel(patrickModel_, patrickModel, view, projection, lightSpace, glm::vec3(0.76f, 0.48f, 0.38f), scene.getOutlineThickness(), 2.6f, true, scene.isToonShadingEnabled());
    renderModel(squidwardModel_, squidwardModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), scene.getOutlineThickness(), 4.4f, true, scene.isToonShadingEnabled());
    renderModel(characterModel_, characterModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), scene.getOutlineThickness(), 4.4f, true, scene.isToonShadingEnabled());
    renderOutlineSlider(scene);
    renderToonToggle(scene);
}

void Renderer::shutdown()
{
    deleteUiResources();
    deleteShadowResources();
    spongebobFallbackTexture_.destroy();
    squidwardModel_.destroy();
    patrickModel_.destroy();
    spongebobModel_.destroy();
    sandModel_.destroy();
    characterModel_.destroy();
    deleteMesh(sphere_);
    glDeleteProgram(shadowProgram_);
    glDeleteProgram(uiProgram_);
    glDeleteProgram(outlineProgram_);
    glDeleteProgram(toonProgram_);
    sphere_ = {};
    shadowProgram_ = 0;
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

Renderer::Mesh Renderer::createSandMesh(float size) const
{
    const float halfSize = size * 0.5f;
    const float vertices[] = {
        -halfSize, 0.0f, -halfSize, 0.0f, 1.0f, 0.0f,
         halfSize, 0.0f, -halfSize, 0.0f, 1.0f, 0.0f,
         halfSize, 0.0f,  halfSize, 0.0f, 1.0f, 0.0f,
        -halfSize, 0.0f,  halfSize, 0.0f, 1.0f, 0.0f
    };
    const unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    Mesh mesh;
    mesh.indexCount = 6;

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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

bool Renderer::createShadowResources()
{
    glGenFramebuffers(1, &shadowFbo_);
    glGenTextures(1, &shadowDepthTexture_);

    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowMapSize, kShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool isComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return isComplete;
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

void Renderer::deleteShadowResources()
{
    glDeleteTextures(1, &shadowDepthTexture_);
    glDeleteFramebuffers(1, &shadowFbo_);
    shadowDepthTexture_ = 0;
    shadowFbo_ = 0;
}

void Renderer::renderSphere(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& baseColor, float outlineThickness, bool useToonShading) const
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
    setMat4(toonProgram_, "uLightSpaceMatrix", glm::mat4(1.0f));
    setVec3(toonProgram_, "uBaseColor", baseColor);
    setVec3(toonProgram_, "uLightDirection", kLightDirection);
    setVec3(toonProgram_, "uAmbientColor", glm::vec3(0.08f, 0.18f, 0.22f));
    setInt(toonProgram_, "uReceiveShadow", 0);
    setInt(toonProgram_, "uUseToonShading", useToonShading ? 1 : 0);
    setInt(toonProgram_, "uUseMaterialColor", 0);
    setInt(toonProgram_, "uUseDiffuseTexture", 0);
    setFloat(toonProgram_, "uMaterialBrightness", 1.0f);
    drawSphere();

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

void Renderer::renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool receiveShadow, bool useToonShading) const
{
    if (!assetModel.isLoaded())
    {
        return;
    }

    glEnable(GL_CULL_FACE);

    if (outlineThickness > 0.0f)
    {
        glCullFace(GL_FRONT);
        glUseProgram(outlineProgram_);
        setMat4(outlineProgram_, "uModel", model);
        setMat4(outlineProgram_, "uView", view);
        setMat4(outlineProgram_, "uProjection", projection);
        setFloat(outlineProgram_, "uOutlineThickness", outlineThickness);
        setVec3(outlineProgram_, "uOutlineColor", glm::vec3(0.0f, 0.04f, 0.22f));
        assetModel.draw();
    }

    glCullFace(GL_BACK);
    glUseProgram(toonProgram_);
    setMat4(toonProgram_, "uModel", model);
    setMat4(toonProgram_, "uView", view);
    setMat4(toonProgram_, "uProjection", projection);
    setMat4(toonProgram_, "uLightSpaceMatrix", lightSpace);
    setVec3(toonProgram_, "uBaseColor", baseColor);
    setVec3(toonProgram_, "uLightDirection", kLightDirection);
    setVec3(toonProgram_, "uAmbientColor", glm::vec3(0.18f, 0.30f, 0.34f));
    setInt(toonProgram_, "uReceiveShadow", receiveShadow ? 1 : 0);
    setInt(toonProgram_, "uUseToonShading", useToonShading ? 1 : 0);
    setInt(toonProgram_, "uUseMaterialColor", 1);
    setInt(toonProgram_, "uUseDiffuseTexture", 0);
    setFloat(toonProgram_, "uMaterialBrightness", materialBrightness);
    setInt(toonProgram_, "uShadowMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    
    assetModel.draw();
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

void Renderer::renderMesh(const Mesh& mesh, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, bool receiveShadow, bool useToonShading) const
{
    glUseProgram(toonProgram_);
    setMat4(toonProgram_, "uModel", model);
    setMat4(toonProgram_, "uView", view);
    setMat4(toonProgram_, "uProjection", projection);
    setMat4(toonProgram_, "uLightSpaceMatrix", lightSpace);
    setVec3(toonProgram_, "uBaseColor", baseColor);
    setVec3(toonProgram_, "uLightDirection", kLightDirection);
    setVec3(toonProgram_, "uAmbientColor", glm::vec3(0.08f, 0.18f, 0.22f));
    setInt(toonProgram_, "uReceiveShadow", receiveShadow ? 1 : 0);
    setInt(toonProgram_, "uUseToonShading", useToonShading ? 1 : 0);
    setInt(toonProgram_, "uUseMaterialColor", 0);
    setInt(toonProgram_, "uUseDiffuseTexture", 0);
    setFloat(toonProgram_, "uMaterialBrightness", 1.0f);
    setInt(toonProgram_, "uShadowMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    drawMesh(mesh);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void Renderer::renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform) const
{
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shadowProgram_);
    renderModelShadowCaster(spongebobModel_, lightSpace, spongebobTransform);
    renderModelShadowCaster(patrickModel_, lightSpace, patrickTransform);
    renderModelShadowCaster(squidwardModel_, lightSpace, squidwardTransform);
    renderModelShadowCaster(characterModel_, lightSpace, characterTransform);
    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderShadowCaster(const glm::mat4& lightSpace, const glm::mat4& model) const
{
    setMat4(shadowProgram_, "uLightSpaceMatrix", lightSpace);
    setMat4(shadowProgram_, "uModel", model);
    drawSphere();
}

void Renderer::renderModelShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const
{
    if (!assetModel.isLoaded())
    {
        return;
    }

    setMat4(shadowProgram_, "uLightSpaceMatrix", lightSpace);
    setMat4(shadowProgram_, "uModel", model);
    assetModel.draw();
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

void Renderer::renderToonToggle(const Scene& scene) const
{
    const float x = Scene::kToonToggleX;
    const float y = Scene::kToonToggleY;
    const float width = Scene::kToonToggleWidth;
    const float height = Scene::kToonToggleHeight;
    const bool isEnabled = scene.isToonShadingEnabled();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(uiProgram_);
    glUniform2f(glGetUniformLocation(uiProgram_, "uScreenSize"), scene.getFramebufferWidth(), scene.getFramebufferHeight());

    drawUiQuad(x - 4.0f, y - 4.0f, width + 8.0f, height + 8.0f, glm::vec3(0.03f, 0.11f, 0.18f));
    drawUiQuad(x, y, width, height, isEnabled ? glm::vec3(0.0f, 0.16f, 0.44f) : glm::vec3(0.12f, 0.16f, 0.19f));
    drawUiText(x + 9.0f, y + 7.0f, isEnabled ? "TOON ON" : "TOON OFF", 2.0f, isEnabled ? glm::vec3(0.92f, 0.95f, 1.0f) : glm::vec3(0.62f, 0.68f, 0.72f));

    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawSphere() const
{
    drawMesh(sphere_);
}

void Renderer::drawMesh(const Mesh& mesh) const
{
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
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

void Renderer::drawUiText(float x, float y, const char* text, float scale, const glm::vec3& color) const
{
    float cursorX = x;
    for (const char* character = text; *character != '\0'; ++character)
    {
        if (*character == ' ')
        {
            cursorX += 4.0f * scale;
            continue;
        }

        drawUiGlyph(cursorX, y, *character, scale, color);
        cursorX += 6.0f * scale;
    }
}

void Renderer::drawUiGlyph(float x, float y, char glyph, float scale, const glm::vec3& color) const
{
    const char* pattern[7] = {};
    switch (glyph)
    {
        case 'T':
        {
            static const char* t[] = {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
            for (int i = 0; i < 7; ++i) pattern[i] = t[i];
            break;
        }
        case 'O':
        {
            static const char* o[] = {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
            for (int i = 0; i < 7; ++i) pattern[i] = o[i];
            break;
        }
        case 'N':
        {
            static const char* n[] = {"10001", "11001", "10101", "10011", "10001", "10001", "10001"};
            for (int i = 0; i < 7; ++i) pattern[i] = n[i];
            break;
        }
        case 'F':
        {
            static const char* f[] = {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
            for (int i = 0; i < 7; ++i) pattern[i] = f[i];
            break;
        }
        default:
            return;
    }

    for (int row = 0; row < 7; ++row)
    {
        for (int column = 0; column < 5; ++column)
        {
            if (pattern[row][column] == '1')
            {
                drawUiQuad(x + static_cast<float>(column) * scale, y + static_cast<float>(row) * scale, scale, scale, color);
            }
        }
    }
}

glm::mat4 Renderer::createLightSpaceMatrix() const
{
    const glm::vec3 lightPosition = -kLightDirection * 6.0f;
    const glm::mat4 lightView = glm::lookAt(
        lightPosition,
        glm::vec3(0.0f, -0.4f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    const glm::mat4 lightProjection = glm::ortho(-5.5f, 5.5f, -5.5f, 5.5f, 0.1f, 14.0f);
    return lightProjection * lightView;
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

void Renderer::setInt(GLuint program, const char* name, int value) const
{
    glUniform1i(glGetUniformLocation(program, name), value);
}
