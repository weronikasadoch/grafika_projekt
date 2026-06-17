#pragma once

#include <GL/glew.h>
#include <glm.hpp>

#include "Model.h"

#include <array>

class Scene;

class Renderer
{
public:
    bool initialize();
    void render(const Scene& scene);
    void shutdown();

private:
    void renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool receiveShadow, bool useToonShading, const Texture* diffuseTexture = nullptr, bool useMaterialColor = true, bool useEmission = false, const glm::vec3& ambientColor = glm::vec3(0.18f, 0.30f, 0.34f), float metallic = 0.0f, float roughness = 0.75f, bool useFastPbr = false) const;
    void renderSkybox(const glm::mat4& view, const glm::mat4& projection) const;
    void renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const;
    void renderPointShadowMap(const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const;
    void renderModelShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const;
    void renderPointShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const;
    bool createSkyboxResources();
    bool createUnderwaterCubemap();
    bool createShadowResources();
    void deleteSkyboxResources();
    void deleteShadowResources();
    glm::mat4 createCoralTransform(int index, const Scene& scene) const;
    glm::mat4 createJellyfishTransform(int index, float elapsedTime) const;
    glm::mat4 createLightSpaceMatrix() const;
    void setMat4(GLuint program, const char* name, const glm::mat4& value) const;
    void setVec3(GLuint program, const char* name, const glm::vec3& value) const;
    void setFloat(GLuint program, const char* name, float value) const;
    void setInt(GLuint program, const char* name, int value) const;

    glm::vec3 pointLightPosition_ = glm::vec3(0.0f);
    glm::vec3 pointLightColor_ = glm::vec3(0.40f, 0.95f, 1.0f);
    float pointLightIntensity_ = 0.9f;
    float pointLightRadius_ = 2.4f;
    GLuint toonProgram_ = 0;
    GLuint pbrProgram_ = 0;
    GLuint outlineProgram_ = 0;
    GLuint skyboxProgram_ = 0;
    GLuint shadowProgram_ = 0;
    GLuint pointShadowProgram_ = 0;
    GLuint shadowFbo_ = 0;
    GLuint shadowDepthTexture_ = 0;
    GLuint pointShadowFbo_ = 0;
    GLuint pointShadowCubemap_ = 0;
    GLuint skyboxVao_ = 0;
    GLuint skyboxVbo_ = 0;
    GLuint skyboxCubemap_ = 0;
    Model sandModel_;
    Model spongebobModel_;
    Model patrickModel_;
    Model squidwardModel_;
    Model characterModel_;
    Model jellyfishModel_;
    std::array<Model, 10> coralModels_;
    Texture spongebobFallbackTexture_;
    Texture spongebobTexture_;
};
