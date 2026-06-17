#include "Renderer.h"

#include "Scene.h"
#include "Shader_Loader.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr int kShadowMapSize = 2048;
    constexpr int kPointShadowMapSize = 512;
    constexpr int kJellyfishCount = 10;
    constexpr int kLightJellyfishIndex = 2;
    constexpr float kPointLightNearPlane = 0.05f;
    constexpr float kPointLightFarPlane = 8.0f;
    const glm::vec3 kLightDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
}

bool Renderer::initialize()
{
    glEnable(GL_DEPTH_TEST);

    Core::Shader_Loader shaderLoader;
    char toonVertexShaderPath[] = "shaders/toon.vert";
    char toonFragmentShaderPath[] = "shaders/toon.frag";
    char pbrVertexShaderPath[] = "shaders/pbr.vert";
    char pbrFragmentShaderPath[] = "shaders/pbr.frag";
    char outlineVertexShaderPath[] = "shaders/outline.vert";
    char outlineFragmentShaderPath[] = "shaders/outline.frag";
    char uiVertexShaderPath[] = "shaders/ui.vert";
    char uiFragmentShaderPath[] = "shaders/ui.frag";
    char skyboxVertexShaderPath[] = "shaders/skybox.vert";
    char skyboxFragmentShaderPath[] = "shaders/skybox.frag";
    char shadowVertexShaderPath[] = "shaders/shadow_depth.vert";
    char shadowFragmentShaderPath[] = "shaders/shadow_depth.frag";
    char pointShadowVertexShaderPath[] = "shaders/point_shadow_depth.vert";
    char pointShadowFragmentShaderPath[] = "shaders/point_shadow_depth.frag";
    toonProgram_ = shaderLoader.CreateProgram(toonVertexShaderPath, toonFragmentShaderPath);
    pbrProgram_ = shaderLoader.CreateProgram(pbrVertexShaderPath, pbrFragmentShaderPath);
    outlineProgram_ = shaderLoader.CreateProgram(outlineVertexShaderPath, outlineFragmentShaderPath);
    uiProgram_ = shaderLoader.CreateProgram(uiVertexShaderPath, uiFragmentShaderPath);
    skyboxProgram_ = shaderLoader.CreateProgram(skyboxVertexShaderPath, skyboxFragmentShaderPath);
    shadowProgram_ = shaderLoader.CreateProgram(shadowVertexShaderPath, shadowFragmentShaderPath);
    pointShadowProgram_ = shaderLoader.CreateProgram(pointShadowVertexShaderPath, pointShadowFragmentShaderPath);
    sphere_ = createSphereMesh(1.0f, 48, 24);
    const bool sandLoaded = sandModel_.loadFromObj("assets/models/scene/sand.obj");
    const bool spongebobLoaded = spongebobModel_.loadFromObj("assets/models/houses/spongebob/spongebob_house_1.obj");
    const bool patrickLoaded = patrickModel_.loadFromObj("assets/models/houses/patrick/patrick_house_1.obj");
    const bool squidwardLoaded = squidwardModel_.loadFromObj("assets/models/houses/squidward/squidward_house_1.obj");
    const bool characterLoaded = characterModel_.loadFromObj("assets/models/Spongebob_model/spongebob_model.obj");
    const bool jellyfishLoaded = jellyfishModel_.loadFromObj("assets/models/Jellyfish_model/jellyfish_model.obj");
    const bool spongebobTextureLoaded = spongebobTexture_.loadPPM("assets/models/Spongebob_model/spongebob.ppm");
    spongebobFallbackTexture_.createSolidColor(255, 214, 54);
    createUiResources();
    const bool skyboxResourcesCreated = createSkyboxResources();
    const bool shadowResourcesCreated = createShadowResources();

    return toonProgram_ != 0 &&
        pbrProgram_ != 0 &&
        outlineProgram_ != 0 &&
        uiProgram_ != 0 &&
        skyboxProgram_ != 0 &&
        shadowProgram_ != 0 &&
        pointShadowProgram_ != 0 &&
        sphere_.vao != 0 &&
        sandLoaded &&
        spongebobLoaded &&
        patrickLoaded &&
        squidwardLoaded &&
        characterLoaded &&
        jellyfishLoaded &&
        spongebobTextureLoaded &&
        uiVao_ != 0 &&
        skyboxResourcesCreated &&
        shadowResourcesCreated;
}

void Renderer::render(const Scene& scene)
{
    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());
    const glm::mat4 lightSpace = createLightSpaceMatrix();
    const float elapsedTime = scene.getElapsedTime();

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
    pointLightPosition_ = glm::vec3(createJellyfishTransform(kLightJellyfishIndex, elapsedTime) * glm::vec4(0.0f, 0.45f, 0.0f, 1.0f));

    //const glm::mat4 characterModel = glm::scale(
      //  glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -1.0f)),
        //glm::vec3(0.45f)
    //);

    renderShadowMap(lightSpace, spongebobModel, patrickModel, squidwardModel, characterModel, elapsedTime);
    renderPointShadowMap(spongebobModel, patrickModel, squidwardModel, characterModel, elapsedTime);
    

    glViewport(0, 0, static_cast<GLsizei>(scene.getFramebufferWidth()), static_cast<GLsizei>(scene.getFramebufferHeight()));
    glClearColor(0.01f, 0.12f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSkybox(view, projection);

    const float outlineThickness = scene.getOutlineThickness();
    const float smallModelOutlineThickness = outlineThickness * 0.32f;

    renderModel(sandModel_, sand, view, projection, lightSpace, glm::vec3(0.86f, 0.68f, 0.38f), 0.0f, 0.92f, true, false, nullptr, true, false, glm::vec3(0.04f, 0.12f, 0.13f), 0.0f, 0.96f, true);
    renderModel(spongebobModel_, spongebobModel, view, projection, lightSpace, glm::vec3(1.0f, 0.72f, 0.20f), outlineThickness, 2.6f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.58f);
    renderModel(patrickModel_, patrickModel, view, projection, lightSpace, glm::vec3(0.76f, 0.48f, 0.38f), outlineThickness, 2.6f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.88f);
    renderModel(squidwardModel_, squidwardModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), outlineThickness, 4.4f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.15f, 0.42f);
    renderModel(characterModel_, characterModel, view, projection, lightSpace, glm::vec3(1.0f), smallModelOutlineThickness, 1.0f, true, scene.isToonShadingEnabled(), &spongebobTexture_, false, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.62f);
    for (int i = 0; i < kJellyfishCount; ++i)
    {
        const bool isLightSource = i == kLightJellyfishIndex;
        const glm::vec3 jellyfishColor = isLightSource ? glm::vec3(0.45f, 0.95f, 1.0f) : glm::vec3(1.0f, 0.42f, 0.78f);
        renderModel(jellyfishModel_, createJellyfishTransform(i, elapsedTime), view, projection, lightSpace, jellyfishColor, smallModelOutlineThickness, isLightSource ? 2.2f : 1.6f, true, scene.isToonShadingEnabled(), nullptr, false, isLightSource, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, isLightSource ? 0.18f : 0.35f);
    }
    renderOutlineSlider(scene);
    renderToonToggle(scene);
}

void Renderer::shutdown()
{
    deleteUiResources();
    deleteSkyboxResources();
    deleteShadowResources();
    spongebobTexture_.destroy();
    spongebobFallbackTexture_.destroy();
    jellyfishModel_.destroy();
    squidwardModel_.destroy();
    patrickModel_.destroy();
    spongebobModel_.destroy();
    sandModel_.destroy();
    characterModel_.destroy();
    deleteMesh(sphere_);
    glDeleteProgram(shadowProgram_);
    glDeleteProgram(pointShadowProgram_);
    glDeleteProgram(uiProgram_);
    glDeleteProgram(skyboxProgram_);
    glDeleteProgram(outlineProgram_);
    glDeleteProgram(pbrProgram_);
    glDeleteProgram(toonProgram_);
    sphere_ = {};
    shadowProgram_ = 0;
    pointShadowProgram_ = 0;
    uiProgram_ = 0;
    skyboxProgram_ = 0;
    outlineProgram_ = 0;
    pbrProgram_ = 0;
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

bool Renderer::createSkyboxResources()
{
    const float vertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVao_);
    glGenBuffers(1, &skyboxVbo_);

    glBindVertexArray(skyboxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return skyboxVao_ != 0 && skyboxVbo_ != 0 && createUnderwaterCubemap();
}

bool Renderer::createUnderwaterCubemap()
{
    constexpr int size = 64;
    constexpr int channels = 3;
    std::array<unsigned char, size * size * channels> pixels = {};

    glGenTextures(1, &skyboxCubemap_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap_);

    for (int face = 0; face < 6; ++face)
    {
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                glm::vec3 direction;

                switch (face)
                {
                    case 0: direction = glm::vec3(1.0f, -v, -u); break;
                    case 1: direction = glm::vec3(-1.0f, -v, u); break;
                    case 2: direction = glm::vec3(u, 1.0f, v); break;
                    case 3: direction = glm::vec3(u, -1.0f, -v); break;
                    case 4: direction = glm::vec3(u, -v, 1.0f); break;
                    default: direction = glm::vec3(-u, -v, -1.0f); break;
                }

                direction = glm::normalize(direction);
                const float height = direction.y * 0.5f + 0.5f;
                const float surfaceLight = std::max(direction.y, 0.0f);
                const float waveLines = std::sin((direction.x * 16.0f + direction.z * 9.0f) + std::sin(direction.z * 18.0f) * 0.7f);
                const float caustics = std::max(waveLines, 0.0f) * surfaceLight * surfaceLight;

                glm::vec3 deepColor(0.005f, 0.08f, 0.13f);
                glm::vec3 midColor(0.02f, 0.26f, 0.34f);
                glm::vec3 surfaceColor(0.22f, 0.72f, 0.78f);
                glm::vec3 color = glm::mix(deepColor, midColor, height);
                color = glm::mix(color, surfaceColor, surfaceLight * 0.55f);
                color += glm::vec3(0.05f, 0.18f, 0.16f) * caustics;

                const int index = (y * size + x) * channels;
                pixels[index + 0] = static_cast<unsigned char>(std::min(color.r, 1.0f) * 255.0f);
                pixels[index + 1] = static_cast<unsigned char>(std::min(color.g, 1.0f) * 255.0f);
                pixels[index + 2] = static_cast<unsigned char>(std::min(color.b, 1.0f) * 255.0f);
            }
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return skyboxCubemap_ != 0;
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

    glGenFramebuffers(1, &pointShadowFbo_);
    glGenTextures(1, &pointShadowCubemap_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowCubemap_);
    for (int face = 0; face < 6; ++face)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT, kPointShadowMapSize, kPointShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFbo_);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, pointShadowCubemap_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool pointShadowComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return isComplete && pointShadowComplete;
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

void Renderer::deleteSkyboxResources()
{
    glDeleteTextures(1, &skyboxCubemap_);
    glDeleteBuffers(1, &skyboxVbo_);
    glDeleteVertexArrays(1, &skyboxVao_);
    skyboxCubemap_ = 0;
    skyboxVbo_ = 0;
    skyboxVao_ = 0;
}

void Renderer::deleteShadowResources()
{
    glDeleteTextures(1, &pointShadowCubemap_);
    glDeleteFramebuffers(1, &pointShadowFbo_);
    glDeleteTextures(1, &shadowDepthTexture_);
    glDeleteFramebuffers(1, &shadowFbo_);
    pointShadowCubemap_ = 0;
    pointShadowFbo_ = 0;
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
    setVec3(toonProgram_, "uPointLightPosition", pointLightPosition_);
    setVec3(toonProgram_, "uPointLightColor", pointLightColor_);
    setFloat(toonProgram_, "uPointLightIntensity", pointLightIntensity_);
    setFloat(toonProgram_, "uPointLightRadius", pointLightRadius_);
    setFloat(toonProgram_, "uPointLightFarPlane", kPointLightFarPlane);
    setInt(toonProgram_, "uUseEmission", 0);
    setInt(toonProgram_, "uPointShadowMap", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowCubemap_);
    drawSphere();
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

void Renderer::renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool receiveShadow, bool useToonShading, const Texture* diffuseTexture, bool useMaterialColor, bool useEmission, const glm::vec3& ambientColor, float metallic, float roughness, bool useFastPbr) const
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

    if (!useToonShading)
    {
        glCullFace(GL_BACK);
        glUseProgram(pbrProgram_);
        setMat4(pbrProgram_, "uModel", model);
        setMat4(pbrProgram_, "uView", view);
        setMat4(pbrProgram_, "uProjection", projection);
        setMat4(pbrProgram_, "uLightSpaceMatrix", lightSpace);
        setVec3(pbrProgram_, "uBaseColor", baseColor);
        setVec3(pbrProgram_, "uCameraPosition", glm::vec3(glm::inverse(view)[3]));
        setVec3(pbrProgram_, "uLightDirection", kLightDirection);
        setVec3(pbrProgram_, "uLightColor", glm::vec3(1.25f, 1.38f, 1.32f));
        setVec3(pbrProgram_, "uAmbientColor", ambientColor);
        setInt(pbrProgram_, "uReceiveShadow", receiveShadow ? 1 : 0);
        setInt(pbrProgram_, "uUseMaterialColor", useMaterialColor ? 1 : 0);
        setInt(pbrProgram_, "uUseDiffuseTexture", diffuseTexture != nullptr ? 1 : 0);
        setFloat(pbrProgram_, "uMaterialBrightness", materialBrightness);
        setFloat(pbrProgram_, "uMetallic", metallic);
        setFloat(pbrProgram_, "uRoughness", roughness);
        setFloat(pbrProgram_, "uAo", 1.0f);
        setInt(pbrProgram_, "uUseFastPbr", useFastPbr ? 1 : 0);
        setVec3(pbrProgram_, "uPointLightPosition", pointLightPosition_);
        setVec3(pbrProgram_, "uPointLightColor", pointLightColor_);
        setFloat(pbrProgram_, "uPointLightIntensity", pointLightIntensity_);
        setFloat(pbrProgram_, "uPointLightRadius", pointLightRadius_);
        setFloat(pbrProgram_, "uPointLightFarPlane", kPointLightFarPlane);
        setInt(pbrProgram_, "uUseEmission", useEmission ? 1 : 0);
        setInt(pbrProgram_, "uShadowMap", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
        if (diffuseTexture != nullptr)
        {
            setInt(pbrProgram_, "uDiffuseTexture", 1);
            diffuseTexture->bind(GL_TEXTURE1);
        }
        setInt(pbrProgram_, "uPointShadowMap", 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowCubemap_);
        glActiveTexture(GL_TEXTURE0);

        assetModel.draw();

        if (diffuseTexture != nullptr)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDisable(GL_CULL_FACE);
        return;
    }

    glCullFace(GL_BACK);
    glUseProgram(toonProgram_);
    setMat4(toonProgram_, "uModel", model);
    setMat4(toonProgram_, "uView", view);
    setMat4(toonProgram_, "uProjection", projection);
    setMat4(toonProgram_, "uLightSpaceMatrix", lightSpace);
    setVec3(toonProgram_, "uBaseColor", baseColor);
    setVec3(toonProgram_, "uLightDirection", kLightDirection);
    setVec3(toonProgram_, "uAmbientColor", ambientColor);
    setInt(toonProgram_, "uReceiveShadow", receiveShadow ? 1 : 0);
    setInt(toonProgram_, "uUseToonShading", useToonShading ? 1 : 0);
    setInt(toonProgram_, "uUseMaterialColor", useMaterialColor ? 1 : 0);
    setInt(toonProgram_, "uUseDiffuseTexture", diffuseTexture != nullptr ? 1 : 0);
    setFloat(toonProgram_, "uMaterialBrightness", materialBrightness);
    setVec3(toonProgram_, "uPointLightPosition", pointLightPosition_);
    setVec3(toonProgram_, "uPointLightColor", pointLightColor_);
    setFloat(toonProgram_, "uPointLightIntensity", pointLightIntensity_);
    setFloat(toonProgram_, "uPointLightRadius", pointLightRadius_);
    setFloat(toonProgram_, "uPointLightFarPlane", kPointLightFarPlane);
    setInt(toonProgram_, "uUseEmission", useEmission ? 1 : 0);
    setInt(toonProgram_, "uShadowMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    setInt(toonProgram_, "uPointShadowMap", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowCubemap_);
    glActiveTexture(GL_TEXTURE0);
    if (diffuseTexture != nullptr)
    {
        setInt(toonProgram_, "uDiffuseTexture", 1);
        diffuseTexture->bind(GL_TEXTURE1);
    }
    
    assetModel.draw();
    if (diffuseTexture != nullptr)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
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
    setVec3(toonProgram_, "uPointLightPosition", pointLightPosition_);
    setVec3(toonProgram_, "uPointLightColor", pointLightColor_);
    setFloat(toonProgram_, "uPointLightIntensity", pointLightIntensity_);
    setFloat(toonProgram_, "uPointLightRadius", pointLightRadius_);
    setFloat(toonProgram_, "uPointLightFarPlane", kPointLightFarPlane);
    setInt(toonProgram_, "uUseEmission", 0);
    setInt(toonProgram_, "uShadowMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    setInt(toonProgram_, "uPointShadowMap", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowCubemap_);
    glActiveTexture(GL_TEXTURE0);
    drawMesh(mesh);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void Renderer::renderSkybox(const glm::mat4& view, const glm::mat4& projection) const
{
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    glUseProgram(skyboxProgram_);
    setMat4(skyboxProgram_, "uView", view);
    setMat4(skyboxProgram_, "uProjection", projection);
    setInt(skyboxProgram_, "uSkybox", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap_);
    glBindVertexArray(skyboxVao_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glUseProgram(0);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void Renderer::renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, float elapsedTime) const
{
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shadowProgram_);
    renderModelShadowCaster(spongebobModel_, lightSpace, spongebobTransform);
    renderModelShadowCaster(patrickModel_, lightSpace, patrickTransform);
    renderModelShadowCaster(squidwardModel_, lightSpace, squidwardTransform);
    renderModelShadowCaster(characterModel_, lightSpace, characterTransform);
    for (int i = 0; i < kJellyfishCount; ++i)
    {
        renderModelShadowCaster(jellyfishModel_, lightSpace, createJellyfishTransform(i, elapsedTime));
    }
    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderPointShadowMap(const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, float elapsedTime) const
{
    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, kPointLightNearPlane, kPointLightFarPlane);
    const glm::vec3 position = pointLightPosition_;
    const glm::mat4 lightViews[] = {
        glm::lookAt(position, position + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(position, position + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(position, position + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(position, position + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(position, position + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(position, position + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    glViewport(0, 0, kPointShadowMapSize, kPointShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFbo_);
    glUseProgram(pointShadowProgram_);
    setVec3(pointShadowProgram_, "uPointLightPosition", pointLightPosition_);
    setFloat(pointShadowProgram_, "uFarPlane", kPointLightFarPlane);

    for (int face = 0; face < 6; ++face)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, pointShadowCubemap_, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        const glm::mat4 lightSpace = projection * lightViews[face];
        renderPointShadowCaster(spongebobModel_, lightSpace, spongebobTransform);
        renderPointShadowCaster(patrickModel_, lightSpace, patrickTransform);
        renderPointShadowCaster(squidwardModel_, lightSpace, squidwardTransform);
        renderPointShadowCaster(characterModel_, lightSpace, characterTransform);
        for (int i = 0; i < kJellyfishCount; ++i)
        {
            if (i == kLightJellyfishIndex)
            {
                continue;
            }
            renderPointShadowCaster(jellyfishModel_, lightSpace, createJellyfishTransform(i, elapsedTime));
        }
    }

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

void Renderer::renderPointShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const
{
    if (!assetModel.isLoaded())
    {
        return;
    }

    setMat4(pointShadowProgram_, "uLightSpaceMatrix", lightSpace);
    setMat4(pointShadowProgram_, "uModel", model);
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
    drawUiQuad(x, y, width, height, isEnabled ? glm::vec3(0.0f, 0.16f, 0.44f) : glm::vec3(0.34f, 0.22f, 0.08f));
    drawUiText(x + 9.0f, y + 7.0f, isEnabled ? "TOON ON" : "PBR ON", 2.0f, isEnabled ? glm::vec3(0.92f, 0.95f, 1.0f) : glm::vec3(0.95f, 0.86f, 0.58f));

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
        case 'L':
        {
            static const char* l[] = {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
            for (int i = 0; i < 7; ++i) pattern[i] = l[i];
            break;
        }
        case 'I':
        {
            static const char* iPattern[] = {"11111", "00100", "00100", "00100", "00100", "00100", "11111"};
            for (int i = 0; i < 7; ++i) pattern[i] = iPattern[i];
            break;
        }
        case 'G':
        {
            static const char* g[] = {"01110", "10001", "10000", "10111", "10001", "10001", "01110"};
            for (int i = 0; i < 7; ++i) pattern[i] = g[i];
            break;
        }
        case 'H':
        {
            static const char* h[] = {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
            for (int i = 0; i < 7; ++i) pattern[i] = h[i];
            break;
        }
        case 'P':
        {
            static const char* p[] = {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
            for (int i = 0; i < 7; ++i) pattern[i] = p[i];
            break;
        }
        case 'B':
        {
            static const char* b[] = {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
            for (int i = 0; i < 7; ++i) pattern[i] = b[i];
            break;
        }
        case 'R':
        {
            static const char* r[] = {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
            for (int i = 0; i < 7; ++i) pattern[i] = r[i];
            break;
        }
        case 'S':
        {
            static const char* s[] = {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
            for (int i = 0; i < 7; ++i) pattern[i] = s[i];
            break;
        }
        case 'A':
        {
            static const char* a[] = {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
            for (int i = 0; i < 7; ++i) pattern[i] = a[i];
            break;
        }
        case 'D':
        {
            static const char* d[] = {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
            for (int i = 0; i < 7; ++i) pattern[i] = d[i];
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

glm::mat4 Renderer::createJellyfishTransform(int index, float elapsedTime) const
{
    static constexpr glm::vec3 basePositions[kJellyfishCount] = {
        glm::vec3(-4.6f, -0.45f, -4.7f),
        glm::vec3(-2.8f, -0.20f, -5.8f),
        glm::vec3(-1.1f,  0.05f, -4.3f),
        glm::vec3( 1.4f, -0.10f, -5.5f),
        glm::vec3( 3.4f, -0.35f, -4.4f),
        glm::vec3( 4.7f,  0.10f, -2.2f),
        glm::vec3( 2.6f,  0.28f, -0.5f),
        glm::vec3( 0.0f,  0.02f, -0.2f),
        glm::vec3(-2.4f,  0.20f, -1.2f),
        glm::vec3(-4.2f, -0.15f, -2.6f)
    };
    static constexpr float radii[kJellyfishCount] = {0.55f, 0.42f, 0.62f, 0.48f, 0.58f, 0.38f, 0.52f, 0.44f, 0.60f, 0.46f};
    static constexpr float speeds[kJellyfishCount] = {0.82f, 0.64f, 0.74f, 0.91f, 0.57f, 0.86f, 0.69f, 0.78f, 0.61f, 0.73f};
    static constexpr float phases[kJellyfishCount] = {0.0f, 1.7f, 3.1f, 4.2f, 5.5f, 0.9f, 2.4f, 3.8f, 5.0f, 1.1f};

    const int i = index % kJellyfishCount;
    const float t = elapsedTime * speeds[i] + phases[i];
    const float pulse = std::sin(t * 3.2f);
    const float x = std::cos(t) * radii[i] + std::sin(t * 0.53f) * radii[i] * 0.45f;
    const float z = std::sin(t * 0.82f) * radii[i] + std::cos(t * 0.47f) * radii[i] * 0.35f;
    const float y = std::sin(t * 1.45f) * 0.18f + pulse * 0.035f;
    const glm::vec3 position = basePositions[i] + glm::vec3(x, y, z);
    const float yaw = std::atan2(std::cos(t * 0.82f) * radii[i] * 0.82f, -std::sin(t) * radii[i]);
    const float tilt = glm::radians(5.0f) * pulse;
    const float scale = 0.54f + 0.035f * pulse;

    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, tilt, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale, scale * (1.0f - 0.05f * pulse), scale));
    return model;
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
