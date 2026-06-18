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
    struct OutlineUniforms
    {
        GLint model = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint outlineThickness = -1;
        GLint outlineColor = -1;
    };

    struct PbrUniforms
    {
        GLint model = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint lightSpaceMatrix = -1;
        GLint baseColor = -1;
        GLint cameraPosition = -1;
        GLint lightDirection = -1;
        GLint lightColor = -1;
        GLint ambientColor = -1;
        GLint useMaterialColor = -1;
        GLint useDiffuseTexture = -1;
        GLint materialBrightness = -1;
        GLint metallic = -1;
        GLint roughness = -1;
        GLint ao = -1;
        GLint useFastPbr = -1;
        GLint useInstancing = -1;
        GLint pointLightPosition = -1;
        GLint pointLightColor = -1;
        GLint pointLightIntensity = -1;
        GLint pointLightRadius = -1;
        GLint useEmission = -1;
        GLint diffuseTexture = -1;
        GLint shadowMap = -1;
    };

    struct ToonUniforms
    {
        GLint model = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint lightSpaceMatrix = -1;
        GLint baseColor = -1;
        GLint lightDirection = -1;
        GLint ambientColor = -1;
        GLint useToonShading = -1;
        GLint useMaterialColor = -1;
        GLint useDiffuseTexture = -1;
        GLint materialBrightness = -1;
        GLint useInstancing = -1;
        GLint pointLightPosition = -1;
        GLint pointLightColor = -1;
        GLint pointLightIntensity = -1;
        GLint pointLightRadius = -1;
        GLint useEmission = -1;
        GLint diffuseTexture = -1;
        GLint shadowMap = -1;
    };

    struct SkyboxUniforms
    {
        GLint view = -1;
        GLint projection = -1;
        GLint skybox = -1;
    };

    struct ShadowUniforms
    {
        GLint model = -1;
        GLint lightSpaceMatrix = -1;
        GLint useInstancing = -1;
    };

    void renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool useToonShading, const Texture* diffuseTexture = nullptr, bool useMaterialColor = true, bool useEmission = false, const glm::vec3& ambientColor = glm::vec3(0.18f, 0.30f, 0.34f), float metallic = 0.0f, float roughness = 0.75f, bool useFastPbr = false) const;
    void renderCoralsInstanced(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace) const;
    void renderSkybox(const glm::mat4& view, const glm::mat4& projection) const;
    void renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const;
    void renderModelShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const;
    void renderCoralShadowCastersInstanced(const glm::mat4& lightSpace) const;
    bool createSkyboxResources();
    bool createUnderwaterCubemap();
    bool createShadowResources();
    void deleteSkyboxResources();
    void deleteShadowResources();
    void cacheUniformLocations();
    GLint getUniformLocation(GLuint program, const char* name) const;
    void initializeCoralTransforms(const Scene& scene);
    const Model& getCoralModel(int modelIndex) const;
    glm::mat4 createRockTransform(const Scene& scene) const;
    glm::mat4 createCoralTransform(int index, const Scene& scene) const;
    glm::mat4 createJellyfishTransform(int index, float elapsedTime) const;
    glm::mat4 createLightSpaceMatrix() const;
    void setMat4(GLint location, const glm::mat4& value) const;
    void setVec3(GLint location, const glm::vec3& value) const;
    void setFloat(GLint location, float value) const;
    void setInt(GLint location, int value) const;

    glm::vec3 pointLightPosition_ = glm::vec3(0.0f);
    glm::vec3 pointLightColor_ = glm::vec3(0.40f, 0.95f, 1.0f);
    float pointLightIntensity_ = 0.9f;
    float pointLightRadius_ = 2.4f;
    GLuint toonProgram_ = 0;
    GLuint pbrProgram_ = 0;
    GLuint outlineProgram_ = 0;
    GLuint skyboxProgram_ = 0;
    GLuint shadowProgram_ = 0;
    OutlineUniforms outlineUniforms_;
    PbrUniforms pbrUniforms_;
    ToonUniforms toonUniforms_;
    SkyboxUniforms skyboxUniforms_;
    ShadowUniforms shadowUniforms_;
    GLuint skyboxVao_ = 0;
    GLuint skyboxVbo_ = 0;
    GLuint skyboxCubemap_ = 0;
    GLuint coralInstanceVbo_ = 0;
    GLuint shadowFbo_ = 0;
    GLuint shadowDepthTexture_ = 0;
    std::array<glm::mat4, 10> coralTransforms_ = {};
    glm::mat4 rockTransform_ = glm::mat4(1.0f);
    float rockTopY_ = 0.0f;
    bool coralTransformsInitialized_ = false;
    Model sandModel_;
    Model spongebobModel_;
    Model patrickModel_;
    Model squidwardModel_;
    Model characterModel_;
    Model jellyfishModel_;
    Model rockModel_;
    Model coral1Model_;
    Model coral2Model_;
    Model coral3Model_;
    Model coral4Model_;
    Model coral5Model_;
    Model coral6Model_;
    Model coral7Model_;
    Model coral8Model_;
    Model coral9Model_;
    Model coral10Model_;
    Texture spongebobFallbackTexture_;
    Texture spongebobTexture_;
};
