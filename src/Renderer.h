#pragma once

#include <GL/glew.h>
#include <glm.hpp>

#include "Model.h"

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

    void renderSphere(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& baseColor, float outlineThickness, bool useToonShading) const;
    void renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool receiveShadow, bool useToonShading, const Texture* diffuseTexture = nullptr, bool useMaterialColor = true) const;
    void renderMesh(const Mesh& mesh, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, bool receiveShadow, bool useToonShading) const;
    void renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const glm::mat4& jellyfishTransform) const;
    void renderShadowCaster(const glm::mat4& lightSpace, const glm::mat4& model) const;
    void renderModelShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const;
    void renderOutlineSlider(const Scene& scene) const;
    void renderToonToggle(const Scene& scene) const;
    Mesh createSphereMesh(float radius, int sectors, int stacks) const;
    Mesh createSandMesh(float size) const;
    void createUiResources();
    bool createShadowResources();
    void deleteMesh(const Mesh& mesh) const;
    void deleteUiResources();
    void deleteShadowResources();
    void drawSphere() const;
    void drawMesh(const Mesh& mesh) const;
    void drawUiQuad(float x, float y, float width, float height, const glm::vec3& color) const;
    void drawUiText(float x, float y, const char* text, float scale, const glm::vec3& color) const;
    void drawUiGlyph(float x, float y, char glyph, float scale, const glm::vec3& color) const;
    glm::mat4 createLightSpaceMatrix() const;
    void setMat4(GLuint program, const char* name, const glm::mat4& value) const;
    void setVec3(GLuint program, const char* name, const glm::vec3& value) const;
    void setFloat(GLuint program, const char* name, float value) const;
    void setInt(GLuint program, const char* name, int value) const;

    GLuint toonProgram_ = 0;
    GLuint outlineProgram_ = 0;
    GLuint uiProgram_ = 0;
    GLuint shadowProgram_ = 0;
    GLuint shadowFbo_ = 0;
    GLuint shadowDepthTexture_ = 0;
    GLuint uiVao_ = 0;
    GLuint uiVbo_ = 0;
    Mesh sphere_;
    Model sandModel_;
    Model spongebobModel_;
    Model patrickModel_;
    Model squidwardModel_;
    Model characterModel_;
    Model jellyfishModel_;
    Texture spongebobFallbackTexture_;
    Texture spongebobTexture_;
};
