#include "Renderer.h"

#include "Scene.h"
#include "Shader_Loader.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr int kJellyfishCount = 10;
    constexpr int kCoralModelCount = 10;
    constexpr int kCoralPlacementCount = 10;
    constexpr int kVisibleCoralInstanceCount = kCoralPlacementCount;
    constexpr int kRockCoralCount = kVisibleCoralInstanceCount;
    constexpr int kShadowMapSize = 2048;
    constexpr int kLightJellyfishIndex = 2;
    constexpr float kRockModelMinY = -0.206605f;
    constexpr float kRockModelMaxY = 0.101145f;
    constexpr float kRockScale = 0.35f;
    constexpr float kRockX = -6.2f;
    constexpr float kRockZ = 6.4f;
    constexpr float kRockCoralEmbedDepth = 0.08f;
    constexpr float kHighFaceCoralLowerOffset = 0.28f;
    const glm::vec3 kLightDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));

    struct DecorationPlacement
    {
        int modelIndex = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
        float scale = 1.0f;
    };

    constexpr DecorationPlacement kCoralPlacements[kCoralPlacementCount] = {
        {0, -4.0f, -0.88f, 6.8f,  18.0f, 1.0f},
        {1, -7.6f, -0.88f, 6.8f, -28.0f, 1.0f},
        {2, -6.0f, -0.90f, 6.8f,  52.0f, 1.0f},
        {3, -4.4f, -0.88f, 6.8f,  95.0f, 1.0f},
        {4, -2.8f, -0.88f, 6.8f, -70.0f, 1.0f},
        {5, -9.2f, -0.90f, 6.0f,  34.0f, 1.2f},
        {6, -7.6f, -0.88f, 6.0f, 142.0f, 1.2f},
        {7, -6.0f, -0.88f, 6.0f, -18.0f, 1.2f},
        {8, -4.4f, -0.88f, 6.0f,  63.0f, 1.2f},
        {9, -2.8f, -0.88f, 8.0f, -92.0f, 1.2f}
    };

    constexpr float kCoralModelMinY[kCoralModelCount] = {
        -0.194700f,
        -0.211738f,
        -0.331710f,
        -0.609442f,
        -0.609442f,
        -0.028204f,
        -0.042306f,
        -0.025383f,
        -0.014655f,
        -0.009475f
    };

    struct EdgeKey
    {
        int first = 0;
        int second = 0;

        bool operator==(const EdgeKey& other) const
        {
            return first == other.first && second == other.second;
        }
    };

    struct EdgeKeyHash
    {
        std::size_t operator()(const EdgeKey& edge) const
        {
            return (static_cast<std::size_t>(edge.first) << 32u) ^ static_cast<std::size_t>(edge.second);
        }
    };

    int parseObjVertexIndex(const std::string& token)
    {
        const std::size_t slash = token.find('/');
        const std::string indexText = slash == std::string::npos ? token : token.substr(0, slash);
        return std::stoi(indexText) - 1;
    }

    void addEdge(std::unordered_map<EdgeKey, int, EdgeKeyHash>& edges, int a, int b)
    {
        if (a == b)
        {
            return;
        }

        EdgeKey edge{std::min(a, b), std::max(a, b)};
        ++edges[edge];
    }

    void drawMaskLine(std::vector<float>& mask, int size, int x0, int y0, int x1, int y1)
    {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        while (true)
        {
            for (int oy = -2; oy <= 2; ++oy)
            {
                for (int ox = -2; ox <= 2; ++ox)
                {
                    const int px = x0 + ox;
                    const int py = y0 + oy;
                    if (px >= 0 && px < size && py >= 0 && py < size)
                    {
                        const float distance = std::sqrt(static_cast<float>(ox * ox + oy * oy));
                        const float coverage = glm::clamp(1.0f - distance / 2.6f, 0.0f, 1.0f);
                        float& pixel = mask[static_cast<std::size_t>(py * size + px)];
                        pixel = std::max(pixel, coverage);
                    }
                }
            }

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            const int doubledError = error * 2;
            if (doubledError >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (doubledError <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    }

    std::vector<float> createFlowerMask(const char* path, int size)
    {
        std::ifstream file(path);
        std::vector<float> mask(static_cast<std::size_t>(size * size), 0.0f);
        if (!file.is_open())
        {
            return mask;
        }

        std::vector<glm::vec2> positions;
        std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeUseCounts;
        glm::vec2 minBounds(std::numeric_limits<float>::max());
        glm::vec2 maxBounds(std::numeric_limits<float>::lowest());
        std::string line;

        while (std::getline(file, line))
        {
            std::istringstream stream(line);
            std::string prefix;
            stream >> prefix;

            if (prefix == "v")
            {
                glm::vec3 position(0.0f);
                stream >> position.x >> position.y >> position.z;
                positions.emplace_back(position.x, position.y);
                minBounds = glm::min(minBounds, positions.back());
                maxBounds = glm::max(maxBounds, positions.back());
            }
            else if (prefix == "f")
            {
                std::vector<int> indices;
                std::string token;
                while (stream >> token)
                {
                    indices.push_back(parseObjVertexIndex(token));
                }

                for (std::size_t i = 0; i < indices.size(); ++i)
                {
                    addEdge(edgeUseCounts, indices[i], indices[(i + 1) % indices.size()]);
                }
            }
        }

        const glm::vec2 boundsSize = maxBounds - minBounds;
        if (positions.empty() || boundsSize.x <= 0.0f || boundsSize.y <= 0.0f)
        {
            return mask;
        }

        const float padding = static_cast<float>(size) * 0.06f;
        const float scale = std::min(
            (static_cast<float>(size) - padding * 2.0f) / boundsSize.x,
            (static_cast<float>(size) - padding * 2.0f) / boundsSize.y
        );
        const glm::vec2 offset(
            (static_cast<float>(size) - boundsSize.x * scale) * 0.5f,
            (static_cast<float>(size) - boundsSize.y * scale) * 0.5f
        );

        for (const auto& edgeEntry : edgeUseCounts)
        {
            if (edgeEntry.second != 1)
            {
                continue;
            }

            const glm::vec2 a = positions[static_cast<std::size_t>(edgeEntry.first.first)];
            const glm::vec2 b = positions[static_cast<std::size_t>(edgeEntry.first.second)];
            const int x0 = static_cast<int>((a.x - minBounds.x) * scale + offset.x);
            const int y0 = static_cast<int>(static_cast<float>(size) - ((a.y - minBounds.y) * scale + offset.y));
            const int x1 = static_cast<int>((b.x - minBounds.x) * scale + offset.x);
            const int y1 = static_cast<int>(static_cast<float>(size) - ((b.y - minBounds.y) * scale + offset.y));
            drawMaskLine(mask, size, x0, y0, x1, y1);
        }

        return mask;
    }
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
    char skyboxVertexShaderPath[] = "shaders/skybox.vert";
    char skyboxFragmentShaderPath[] = "shaders/skybox.frag";
    char shadowVertexShaderPath[] = "shaders/shadow_depth.vert";
    char shadowFragmentShaderPath[] = "shaders/shadow_depth.frag";
    toonProgram_ = shaderLoader.CreateProgram(toonVertexShaderPath, toonFragmentShaderPath);
    pbrProgram_ = shaderLoader.CreateProgram(pbrVertexShaderPath, pbrFragmentShaderPath);
    outlineProgram_ = shaderLoader.CreateProgram(outlineVertexShaderPath, outlineFragmentShaderPath);
    skyboxProgram_ = shaderLoader.CreateProgram(skyboxVertexShaderPath, skyboxFragmentShaderPath);
    shadowProgram_ = shaderLoader.CreateProgram(shadowVertexShaderPath, shadowFragmentShaderPath);
    const bool shaderProgramsCreated = toonProgram_ != 0 &&
        pbrProgram_ != 0 &&
        outlineProgram_ != 0 &&
        skyboxProgram_ != 0 &&
        shadowProgram_ != 0;
    if (shaderProgramsCreated)
    {
        cacheUniformLocations();
    }
    const bool sandLoaded = sandModel_.loadFromObj("assets/models/scene/sand.obj");
    const bool spongebobLoaded = spongebobModel_.loadFromObj("assets/models/houses/spongebob/spongebob_house_1.obj");
    const bool patrickLoaded = patrickModel_.loadFromObj("assets/models/houses/patrick/patrick_house_1.obj");
    const bool squidwardLoaded = squidwardModel_.loadFromObj("assets/models/houses/squidward/squidward_house_1.obj");
    const bool characterLoaded = characterModel_.loadFromObj("assets/models/Spongebob_model/spongebob_model.obj");
    const bool jellyfishLoaded = jellyfishModel_.loadFromObj("assets/models/Jellyfish_model/jellyfish_model.obj");
    const bool garrLoaded = garyModel_.loadFromObj("assets/models/gary_pet/gary_pet.obj");
    const bool rockLoaded = rockModel_.loadFromObj("assets/models/coral_rock/rock.obj");
    const bool coral1Loaded = coral1Model_.loadFromObj("assets/models/coral_rock/coral_1.obj");
    const bool coral2Loaded = coral2Model_.loadFromObj("assets/models/coral_rock/coral_2.obj");
    const bool coral3Loaded = coral3Model_.loadFromObj("assets/models/coral_rock/coral_3.obj");
    const bool coral4Loaded = coral4Model_.loadFromObj("assets/models/coral_rock/coral_4.obj");
    const bool coral5Loaded = coral5Model_.loadFromObj("assets/models/coral_rock/coral_5.obj");
    const bool coral6Loaded = coral6Model_.loadFromObj("assets/models/coral_rock/coral_6.obj");
    const bool coral7Loaded = coral7Model_.loadFromObj("assets/models/coral_rock/coral_7.obj");
    const bool coral8Loaded = coral8Model_.loadFromObj("assets/models/coral_rock/coral_8.obj");
    const bool coral9Loaded = coral9Model_.loadFromObj("assets/models/coral_rock/coral_9.obj");
    const bool coral10Loaded = coral10Model_.loadFromObj("assets/models/coral_rock/coral_10.obj");
    const bool bubbleLoaded = bubbleModel_.loadFromObj("assets/models/sphere.obj");

    const bool spongebobTextureLoaded = true;
    spongebobFallbackTexture_.createSolidColor(255, 214, 54);
   
    const bool skyboxResourcesCreated = createSkyboxResources();
    const bool shadowResourcesCreated = createShadowResources();
    glGenBuffers(1, &coralInstanceVbo_);

    return shaderProgramsCreated &&
        sandLoaded &&
        spongebobLoaded &&
        patrickLoaded &&
        squidwardLoaded &&
        characterLoaded &&
        jellyfishLoaded &&
        rockLoaded &&
        coral1Loaded &&
        coral2Loaded &&
        coral3Loaded &&
        coral4Loaded &&
        coral5Loaded &&
        coral6Loaded &&
        coral7Loaded &&
        coral8Loaded &&
        coral9Loaded &&
        coral10Loaded &&
        spongebobTextureLoaded &&
        skyboxResourcesCreated &&
        shadowResourcesCreated &&
        coralInstanceVbo_ != 0;
}

void Renderer::render(const Scene& scene)
{
    if (!coralTransformsInitialized_)
    {
        initializeCoralTransforms(scene);
    }

    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());
    const glm::mat4 lightSpace = createLightSpaceMatrix();
    const int jellyfishCount = std::clamp(scene.getJellyfishCount(), 0, kJellyfishCount);

    const glm::mat4 sand = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.95f, 0.0f));
    const glm::mat4 spongebobModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, -1.0f, -2.7)),
        glm::vec3(0.45f)
    );
    const glm::mat4 patrickModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, -0.90f, -2.6f)),
        glm::vec3(0.45f)
    );
    const glm::mat4 squidwardModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -2.6f)),
        glm::vec3(0.45f)
    );
    glm::vec3 charPos = scene.getCharacterPosition();
    float charYaw = scene.getCharacterYaw();

    glm::mat4 characterModel = glm::mat4(1.0f);
    characterModel = glm::translate(characterModel, charPos); 
    characterModel = glm::rotate(characterModel, -charYaw + glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    characterModel = glm::scale(characterModel, glm::vec3(0.15f));
    if (jellyfishCount > kLightJellyfishIndex)
    {
        pointLightPosition_ = glm::vec3(createJellyfishTransform(kLightJellyfishIndex, scene.getJellyfishAnimationTime(kLightJellyfishIndex)) * glm::vec4(0.0f, 0.45f, 0.0f, 1.0f));
        pointLightIntensity_ = 0.9f;
    }
    else
    {
        pointLightIntensity_ = 0.0f;
    }

    const glm::mat4 garyModel = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, -1.0f, -1.0f)),
        glm::vec3(0.001f)
    );

    glm::mat4 bubbleTransform = glm::mat4(1.0f);
    bubbleTransform = glm::translate(bubbleTransform, glm::vec3(0.0f, 1.5f, 0.0f)); // Pozycja X, Y, Z
    bubbleTransform = glm::scale(bubbleTransform, glm::vec3(0.1f));


    renderShadowMap(lightSpace, spongebobModel, patrickModel, squidwardModel, characterModel, scene, jellyfishCount);

    glViewport(0, 0, static_cast<GLsizei>(scene.getFramebufferWidth()), static_cast<GLsizei>(scene.getFramebufferHeight()));
    glClearColor(0.05f, 0.35f, 0.50f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSkybox(view, projection);

    const float outlineThickness = scene.getOutlineThickness();
    const float smallModelOutlineThickness = outlineThickness * 0.32f;

    renderModel(sandModel_, sand, view, projection, lightSpace, glm::vec3(0.878f, 0.890f, 0.741f), 0.0f, 0.92f, false, nullptr, false, false, glm::vec3(0.04f, 0.12f, 0.13f), 0.0f, 0.96f, true);
    renderModel(rockModel_, rockTransform_, view, projection, lightSpace, glm::vec3(0.38f, 0.33f, 0.29f), 0.0f, 1.15f, false, nullptr, true, false, glm::vec3(0.10f, 0.18f, 0.19f), 0.0f, 0.88f);
    renderCoralsInstanced(view, projection, lightSpace);
    renderModel(spongebobModel_, spongebobModel, view, projection, lightSpace, glm::vec3(1.0f, 0.72f, 0.20f), outlineThickness, 2.6f, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.58f);
    renderModel(patrickModel_, patrickModel, view, projection, lightSpace, glm::vec3(0.76f, 0.48f, 0.38f), outlineThickness, 2.6f, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.88f);
    renderModel(squidwardModel_, squidwardModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), outlineThickness, 4.4f, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.15f, 0.42f);
    renderModel(characterModel_, characterModel, view, projection, lightSpace, glm::vec3(1.0f),smallModelOutlineThickness,1.4f, scene.isToonShadingEnabled(), &spongebobFallbackTexture_,false, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.62f);
    renderModel(garyModel_,garyModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), outlineThickness, 4.4f, scene.isToonShadingEnabled(), nullptr, false, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.15f, 0.42f);
    for (int i = 0; i < jellyfishCount; ++i)
    {
        const bool isLightSource = i == kLightJellyfishIndex;
        const glm::vec3 jellyfishColor = isLightSource ? glm::vec3(0.45f, 0.95f, 1.0f) : glm::vec3(1.0f, 0.42f, 0.78f);
        renderModel(jellyfishModel_, createJellyfishTransform(i, scene.getJellyfishAnimationTime(i)), view, projection, lightSpace, jellyfishColor, smallModelOutlineThickness, isLightSource ? 2.2f : 1.6f, scene.isToonShadingEnabled(), nullptr, false, isLightSource, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, isLightSource ? 0.18f : 0.35f);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standardowe mieszanie dla szkła/powietrza
    glDepthMask(GL_FALSE);
    for (const auto& bubble : scene.getBubbles())
    {
        glm::mat4 bubbleTransform = glm::mat4(1.0f);
        bubbleTransform = glm::translate(bubbleTransform, bubble.position);
        bubbleTransform = glm::scale(bubbleTransform, glm::vec3(bubble.size));
        renderModel(bubbleModel_, bubbleTransform, view, projection, lightSpace, glm::vec3(0.5, 0.95, 0.98), 0.0f, 2.5f, false, nullptr, false, false, glm::vec3(0.5f, 0.7f, 0.8f), 0.1f, 0.02f, false);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::shutdown()
{
    glDeleteBuffers(1, &coralInstanceVbo_);
    deleteShadowResources();
    deleteSkyboxResources();
    spongebobTexture_.destroy();
    spongebobFallbackTexture_.destroy();
    jellyfishModel_.destroy();
    rockModel_.destroy();
    coral10Model_.destroy();
    coral9Model_.destroy();
    coral8Model_.destroy();
    coral7Model_.destroy();
    coral6Model_.destroy();
    coral5Model_.destroy();
    coral4Model_.destroy();
    coral3Model_.destroy();
    coral2Model_.destroy();
    coral1Model_.destroy();
    squidwardModel_.destroy();
    patrickModel_.destroy();
    spongebobModel_.destroy();
    sandModel_.destroy();
    characterModel_.destroy();
    garyModel_.destroy();
    bubbleModel_.destroy();
    glDeleteProgram(skyboxProgram_);
    glDeleteProgram(shadowProgram_);
    glDeleteProgram(outlineProgram_);
    glDeleteProgram(pbrProgram_);
    glDeleteProgram(toonProgram_);
    skyboxProgram_ = 0;
    shadowProgram_ = 0;
    outlineProgram_ = 0;
    pbrProgram_ = 0;
    toonProgram_ = 0;
    coralInstanceVbo_ = 0;
    coralTransformsInitialized_ = false;
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
    constexpr int size = 768;
    constexpr int channels = 3;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size * size * channels), 0);
    const std::array<std::vector<float>, 6> flowerMasks = {
        createFlowerMask("assets/models/flowers/flower_1.obj", size),
        createFlowerMask("assets/models/flowers/flower_3.obj", size),
        createFlowerMask("assets/models/flowers/flower_5.obj", size),
        createFlowerMask("assets/models/flowers/flower_7.obj", size),
        createFlowerMask("assets/models/flowers/flower_9.obj", size),
        createFlowerMask("assets/models/flowers/flower_11.obj", size)
    };

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

                glm::vec3 deepColor(0.05f, 0.35f, 0.55f);
                glm::vec3 midColor(0.10f, 0.55f, 0.70f);
                glm::vec3 surfaceColor(0.40f, 0.85f, 0.90f);
                glm::vec3 color = glm::mix(deepColor, midColor, height);
                color = glm::mix(color, surfaceColor, surfaceLight * 0.55f);
                color += glm::vec3(0.05f, 0.18f, 0.16f) * caustics;

                if (!flowerMasks.empty())
                {
                    static constexpr float panelCenters[] = {0.22f, 0.34f, 0.46f, 0.58f, 0.70f, 0.82f};
                    static const glm::vec3 panelColors[] = {
                        glm::vec3(0.98f, 0.78f, 0.28f),
                        glm::vec3(0.95f, 0.25f, 0.52f),
                        glm::vec3(0.20f, 0.95f, 0.78f),
                        glm::vec3(0.92f, 0.55f, 1.00f),
                        glm::vec3(0.98f, 0.40f, 0.18f),
                        glm::vec3(0.72f, 0.98f, 0.30f)
                    };
                    
                    constexpr float panelWidth = 0.095f;
                    constexpr float panelMinY = -0.08f;
                    constexpr float panelMaxY = 0.12f;
                    const float longitude = (std::atan2(direction.x, -direction.z) / (2.0f * kPi)) + 0.5f;

                    for (std::size_t panel = 0; panel < std::size(panelCenters); ++panel)
                    {
                        const std::vector<float>& flowerMask = flowerMasks[panel % flowerMasks.size()];
                        if (flowerMask.empty())
                        {
                            continue;
                        }

                        const float halfWidth = panelWidth * 0.5f;
                        const float horizontalDistance = std::abs(longitude - panelCenters[panel]);
                        if (horizontalDistance > halfWidth || direction.y < panelMinY || direction.y > panelMaxY)
                        {
                            continue;
                        }

                        const float flowerU = (longitude - (panelCenters[panel] - halfWidth)) / panelWidth;
                        const float flowerV = 1.0f - ((direction.y - panelMinY) / (panelMaxY - panelMinY));
                        const int flowerX = glm::clamp(static_cast<int>(flowerU * static_cast<float>(size)), 0, size - 1);
                        const int flowerY = glm::clamp(static_cast<int>(flowerV * static_cast<float>(size)), 0, size - 1);
                        const float maskValue = flowerMask[static_cast<std::size_t>(flowerY * size + flowerX)];

                        if (maskValue > 0.0f)
                        {
                            color = glm::mix(color, panelColors[panel], maskValue);
                        }
                    }
                }

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
    glBindTexture(GL_TEXTURE_2D, 0);
    return isComplete && shadowFbo_ != 0 && shadowDepthTexture_ != 0;
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
    glDeleteTextures(1, &shadowDepthTexture_);
    glDeleteFramebuffers(1, &shadowFbo_);
    shadowDepthTexture_ = 0;
    shadowFbo_ = 0;
}

void Renderer::renderModel(const Model& assetModel, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace, const glm::vec3& baseColor, float outlineThickness, float materialBrightness, bool useToonShading, const Texture* diffuseTexture, bool useMaterialColor, bool useEmission, const glm::vec3& ambientColor, float metallic, float roughness, bool useFastPbr) const
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
        setMat4(outlineUniforms_.model, model);
        setMat4(outlineUniforms_.view, view);
        setMat4(outlineUniforms_.projection, projection);
        setFloat(outlineUniforms_.outlineThickness, outlineThickness);
        setVec3(outlineUniforms_.outlineColor, glm::vec3(0.0f, 0.04f, 0.22f));
        assetModel.draw();
    }

    if (!useToonShading)
    {
        glCullFace(GL_BACK);
        glUseProgram(pbrProgram_);
        setMat4(pbrUniforms_.model, model);
        setMat4(pbrUniforms_.view, view);
        setMat4(pbrUniforms_.projection, projection);
        setMat4(pbrUniforms_.lightSpaceMatrix, lightSpace);
        setVec3(pbrUniforms_.baseColor, baseColor);
        setVec3(pbrUniforms_.cameraPosition, glm::vec3(glm::inverse(view)[3]));
        setVec3(pbrUniforms_.lightDirection, kLightDirection);
        setVec3(pbrUniforms_.lightColor, glm::vec3(2.0f, 2.2f, 2.1f));
        setVec3(pbrUniforms_.ambientColor, ambientColor);
        setInt(pbrUniforms_.useMaterialColor, useMaterialColor ? 1 : 0);
        setInt(pbrUniforms_.useDiffuseTexture, diffuseTexture != nullptr ? 1 : 0);
        setFloat(pbrUniforms_.materialBrightness, materialBrightness);
        setFloat(pbrUniforms_.metallic, metallic);
        setFloat(pbrUniforms_.roughness, roughness);
        setFloat(pbrUniforms_.ao, 1.0f);
        setInt(pbrUniforms_.useFastPbr, useFastPbr ? 1 : 0);
        setInt(pbrUniforms_.useInstancing, 0);
        setVec3(pbrUniforms_.pointLightPosition, pointLightPosition_);
        setVec3(pbrUniforms_.pointLightColor, pointLightColor_);
        setFloat(pbrUniforms_.pointLightIntensity, pointLightIntensity_);
        setFloat(pbrUniforms_.pointLightRadius, pointLightRadius_);
        setInt(pbrUniforms_.useEmission, useEmission ? 1 : 0);
        setInt(pbrUniforms_.shadowMap, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
        if (diffuseTexture != nullptr)
        {
            setInt(pbrUniforms_.diffuseTexture, 0);
            diffuseTexture->bind(GL_TEXTURE0);
        }

        assetModel.draw();

        if (diffuseTexture != nullptr)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
        glDisable(GL_CULL_FACE);
        return;
    }

    glCullFace(GL_BACK);
    glUseProgram(toonProgram_);
    setMat4(toonUniforms_.model, model);
    setMat4(toonUniforms_.view, view);
    setMat4(toonUniforms_.projection, projection);
    setMat4(toonUniforms_.lightSpaceMatrix, lightSpace);
    setVec3(toonUniforms_.baseColor, baseColor);
    setVec3(toonUniforms_.lightDirection, kLightDirection);
    setVec3(toonUniforms_.ambientColor, ambientColor);
    setInt(toonUniforms_.useToonShading, useToonShading ? 1 : 0);
    setInt(toonUniforms_.useMaterialColor, useMaterialColor ? 1 : 0);
    setInt(toonUniforms_.useDiffuseTexture, diffuseTexture != nullptr ? 1 : 0);
    setFloat(toonUniforms_.materialBrightness, materialBrightness);
    setInt(toonUniforms_.useInstancing, 0);
    setVec3(toonUniforms_.pointLightPosition, pointLightPosition_);
    setVec3(toonUniforms_.pointLightColor, pointLightColor_);
    setFloat(toonUniforms_.pointLightIntensity, pointLightIntensity_);
    setFloat(toonUniforms_.pointLightRadius, pointLightRadius_);
    setInt(toonUniforms_.useEmission, useEmission ? 1 : 0);
    setInt(toonUniforms_.shadowMap, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    if (diffuseTexture != nullptr)
    {
        setInt(toonUniforms_.diffuseTexture, 0);
        diffuseTexture->bind(GL_TEXTURE0);
    }

    assetModel.draw();
    if (diffuseTexture != nullptr)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

void Renderer::renderSkybox(const glm::mat4& view, const glm::mat4& projection) const
{
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    glUseProgram(skyboxProgram_);
    setMat4(skyboxUniforms_.view, view);
    setMat4(skyboxUniforms_.projection, projection);
    setInt(skyboxUniforms_.skybox, 0);
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

void Renderer::renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const
{
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(shadowProgram_);
    renderModelShadowCaster(spongebobModel_, lightSpace, spongebobTransform);
    renderModelShadowCaster(patrickModel_, lightSpace, patrickTransform);
    renderModelShadowCaster(squidwardModel_, lightSpace, squidwardTransform);
    renderModelShadowCaster(characterModel_, lightSpace, characterTransform);
    renderModelShadowCaster(rockModel_, lightSpace, rockTransform_);
    renderCoralShadowCastersInstanced(lightSpace);
    for (int i = 0; i < jellyfishCount; ++i)
    {
        renderModelShadowCaster(jellyfishModel_, lightSpace, createJellyfishTransform(i, scene.getJellyfishAnimationTime(i)));
    }
    glUseProgram(0);
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderModelShadowCaster(const Model& assetModel, const glm::mat4& lightSpace, const glm::mat4& model) const
{
    if (!assetModel.isLoaded())
    {
        return;
    }

    setMat4(shadowUniforms_.model, model);
    setMat4(shadowUniforms_.lightSpaceMatrix, lightSpace);
    setInt(shadowUniforms_.useInstancing, 0);
    assetModel.draw();
}

void Renderer::renderCoralShadowCastersInstanced(const glm::mat4& lightSpace) const
{
    std::array<std::vector<glm::mat4>, kCoralModelCount> transformsByModel;
    for (int i = 0; i < kVisibleCoralInstanceCount; ++i)
    {
        const int modelIndex = kCoralPlacements[i].modelIndex % kCoralModelCount;
        transformsByModel[static_cast<std::size_t>(modelIndex)].push_back(coralTransforms_[static_cast<std::size_t>(i)]);
    }

    setMat4(shadowUniforms_.model, glm::mat4(1.0f));
    setMat4(shadowUniforms_.lightSpaceMatrix, lightSpace);
    setInt(shadowUniforms_.useInstancing, 1);

    for (int modelIndex = 0; modelIndex < kCoralModelCount; ++modelIndex)
    {
        const std::vector<glm::mat4>& transforms = transformsByModel[static_cast<std::size_t>(modelIndex)];
        if (transforms.empty())
        {
            continue;
        }

        glBindBuffer(GL_ARRAY_BUFFER, coralInstanceVbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(transforms.size() * sizeof(glm::mat4)),
            transforms.data(),
            GL_DYNAMIC_DRAW
        );
        getCoralModel(modelIndex).drawInstanced(coralInstanceVbo_, static_cast<GLsizei>(transforms.size()));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::renderCoralsInstanced(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpace) const
{
    std::array<std::vector<glm::mat4>, kCoralModelCount> transformsByModel;
    for (int i = 0; i < kVisibleCoralInstanceCount; ++i)
    {
        const int modelIndex = kCoralPlacements[i].modelIndex % kCoralModelCount;
        transformsByModel[static_cast<std::size_t>(modelIndex)].push_back(coralTransforms_[static_cast<std::size_t>(i)]);
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(toonProgram_);
    setMat4(toonUniforms_.model, glm::mat4(1.0f));
    setMat4(toonUniforms_.view, view);
    setMat4(toonUniforms_.projection, projection);
    setMat4(toonUniforms_.lightSpaceMatrix, lightSpace);
    setVec3(toonUniforms_.baseColor, glm::vec3(0.95f, 0.25f, 0.48f));
    setVec3(toonUniforms_.lightDirection, kLightDirection);
    setVec3(toonUniforms_.ambientColor, glm::vec3(0.16f, 0.28f, 0.31f));
    setInt(toonUniforms_.useToonShading, 0);
    setInt(toonUniforms_.useMaterialColor, 1);
    setInt(toonUniforms_.useDiffuseTexture, 0);
    setFloat(toonUniforms_.materialBrightness, 1.45f);
    setInt(toonUniforms_.useInstancing, 1);
    setVec3(toonUniforms_.pointLightPosition, pointLightPosition_);
    setVec3(toonUniforms_.pointLightColor, pointLightColor_);
    setFloat(toonUniforms_.pointLightIntensity, pointLightIntensity_);
    setFloat(toonUniforms_.pointLightRadius, pointLightRadius_);
    setInt(toonUniforms_.useEmission, 0);
    setInt(toonUniforms_.shadowMap, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);

    for (int modelIndex = 0; modelIndex < kCoralModelCount; ++modelIndex)
    {
        const std::vector<glm::mat4>& transforms = transformsByModel[static_cast<std::size_t>(modelIndex)];
        if (transforms.empty())
        {
            continue;
        }

        glBindBuffer(GL_ARRAY_BUFFER, coralInstanceVbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(transforms.size() * sizeof(glm::mat4)),
            transforms.data(),
            GL_DYNAMIC_DRAW
        );
        getCoralModel(modelIndex).drawInstanced(coralInstanceVbo_, static_cast<GLsizei>(transforms.size()));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
}

const Model& Renderer::getCoralModel(int modelIndex) const
{
    switch (modelIndex)
    {
        case 0: return coral1Model_;
        case 1: return coral2Model_;
        case 2: return coral3Model_;
        case 3: return coral4Model_;
        case 4: return coral5Model_;
        case 5: return coral6Model_;
        case 6: return coral7Model_;
        case 7: return coral8Model_;
        case 8: return coral9Model_;
        default: return coral10Model_;
    }
}

void Renderer::initializeCoralTransforms(const Scene& scene)
{
    rockTransform_ = createRockTransform(scene);
    rockTopY_ = scene.getSandHeight(kRockX, kRockZ) - kRockModelMinY * kRockScale + kRockModelMaxY * kRockScale;
    for (int i = 0; i < kCoralPlacementCount; ++i)
    {
        coralTransforms_[static_cast<std::size_t>(i)] = createCoralTransform(i, scene);
    }
    coralTransformsInitialized_ = true;
}

glm::mat4 Renderer::createRockTransform(const Scene& scene) const
{
    const float sandY = scene.getSandHeight(kRockX, kRockZ);
    const float y = sandY - kRockModelMinY * kRockScale;

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(kRockX, y, kRockZ));
    model = glm::rotate(model, glm::radians(-12.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(kRockScale));
    return model;
}

glm::mat4 Renderer::createCoralTransform(int index, const Scene& scene) const
{
    const DecorationPlacement& placement = kCoralPlacements[index % kCoralPlacementCount];
    const int modelIndex = placement.modelIndex % kCoralModelCount;
    const float scale = placement.scale;
    const float highFaceLowerOffset = modelIndex < 5 ? kHighFaceCoralLowerOffset : 0.0f;
    const float sandY = scene.getSandHeight(placement.x, placement.z);
    const float y = index < kRockCoralCount
        ? rockTopY_ - kCoralModelMinY[modelIndex] * scale - kRockCoralEmbedDepth - highFaceLowerOffset
        : sandY - kCoralModelMinY[modelIndex] * scale - highFaceLowerOffset;

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(placement.x, y, placement.z));
    model = glm::rotate(model, glm::radians(placement.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale));
    return model;
}

glm::mat4 Renderer::createJellyfishTransform(int index, float elapsedTime) const
{
    static const glm::vec3 basePositions[kJellyfishCount] = {
    glm::vec3(-4.6f, -0.45f, -4.7f),
    glm::vec3(-2.8f, -0.20f, -5.8f),
    glm::vec3(-1.1f,  0.05f, -4.3f),
    glm::vec3(1.4f, -0.10f, -5.5f),
    glm::vec3(3.4f, -0.35f, -4.4f),
    glm::vec3(4.7f,  0.10f, -2.2f),
    glm::vec3(2.6f,  0.28f, -0.5f),
    glm::vec3(0.0f,  0.02f, -0.2f),
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
    const float baseWidth = 0.48f;
    const float baseHeight = 0.22f;
    const float scaleX = baseWidth * (1.0f + 0.08f * std::max(0.0f, pulse));
    const float scaleY = baseHeight * (1.0f - 0.20f * std::abs(pulse));
    const float scaleZ = baseWidth * (1.0f + 0.08f * std::max(0.0f, pulse));


    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, tilt, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, scaleY, scaleZ));
    return model;
}

glm::mat4 Renderer::createLightSpaceMatrix() const
{
    const glm::vec3 lightTarget(0.0f, -0.4f, 0.0f);
    const glm::vec3 lightPosition = lightTarget - kLightDirection * 18.0f;
    const glm::mat4 lightView = glm::lookAt(
        lightPosition,
        lightTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    const glm::mat4 lightProjection = glm::ortho(-16.0f, 16.0f, -16.0f, 16.0f, 0.1f, 36.0f);
    return lightProjection * lightView;
}

void Renderer::cacheUniformLocations()
{
    outlineUniforms_.model = getUniformLocation(outlineProgram_, "uModel");
    outlineUniforms_.view = getUniformLocation(outlineProgram_, "uView");
    outlineUniforms_.projection = getUniformLocation(outlineProgram_, "uProjection");
    outlineUniforms_.outlineThickness = getUniformLocation(outlineProgram_, "uOutlineThickness");
    outlineUniforms_.outlineColor = getUniformLocation(outlineProgram_, "uOutlineColor");

    pbrUniforms_.model = getUniformLocation(pbrProgram_, "uModel");
    pbrUniforms_.view = getUniformLocation(pbrProgram_, "uView");
    pbrUniforms_.projection = getUniformLocation(pbrProgram_, "uProjection");
    pbrUniforms_.lightSpaceMatrix = getUniformLocation(pbrProgram_, "uLightSpaceMatrix");
    pbrUniforms_.baseColor = getUniformLocation(pbrProgram_, "uBaseColor");
    pbrUniforms_.cameraPosition = getUniformLocation(pbrProgram_, "uCameraPosition");
    pbrUniforms_.lightDirection = getUniformLocation(pbrProgram_, "uLightDirection");
    pbrUniforms_.lightColor = getUniformLocation(pbrProgram_, "uLightColor");
    pbrUniforms_.ambientColor = getUniformLocation(pbrProgram_, "uAmbientColor");
    pbrUniforms_.useMaterialColor = getUniformLocation(pbrProgram_, "uUseMaterialColor");
    pbrUniforms_.useDiffuseTexture = getUniformLocation(pbrProgram_, "uUseDiffuseTexture");
    pbrUniforms_.materialBrightness = getUniformLocation(pbrProgram_, "uMaterialBrightness");
    pbrUniforms_.metallic = getUniformLocation(pbrProgram_, "uMetallic");
    pbrUniforms_.roughness = getUniformLocation(pbrProgram_, "uRoughness");
    pbrUniforms_.ao = getUniformLocation(pbrProgram_, "uAo");
    pbrUniforms_.useFastPbr = getUniformLocation(pbrProgram_, "uUseFastPbr");
    pbrUniforms_.useInstancing = getUniformLocation(pbrProgram_, "uUseInstancing");
    pbrUniforms_.pointLightPosition = getUniformLocation(pbrProgram_, "uPointLightPosition");
    pbrUniforms_.pointLightColor = getUniformLocation(pbrProgram_, "uPointLightColor");
    pbrUniforms_.pointLightIntensity = getUniformLocation(pbrProgram_, "uPointLightIntensity");
    pbrUniforms_.pointLightRadius = getUniformLocation(pbrProgram_, "uPointLightRadius");
    pbrUniforms_.useEmission = getUniformLocation(pbrProgram_, "uUseEmission");
    pbrUniforms_.diffuseTexture = getUniformLocation(pbrProgram_, "uDiffuseTexture");
    pbrUniforms_.shadowMap = getUniformLocation(pbrProgram_, "uShadowMap");

    toonUniforms_.model = getUniformLocation(toonProgram_, "uModel");
    toonUniforms_.view = getUniformLocation(toonProgram_, "uView");
    toonUniforms_.projection = getUniformLocation(toonProgram_, "uProjection");
    toonUniforms_.lightSpaceMatrix = getUniformLocation(toonProgram_, "uLightSpaceMatrix");
    toonUniforms_.baseColor = getUniformLocation(toonProgram_, "uBaseColor");
    toonUniforms_.lightDirection = getUniformLocation(toonProgram_, "uLightDirection");
    toonUniforms_.ambientColor = getUniformLocation(toonProgram_, "uAmbientColor");
    toonUniforms_.useToonShading = getUniformLocation(toonProgram_, "uUseToonShading");
    toonUniforms_.useMaterialColor = getUniformLocation(toonProgram_, "uUseMaterialColor");
    toonUniforms_.useDiffuseTexture = getUniformLocation(toonProgram_, "uUseDiffuseTexture");
    toonUniforms_.materialBrightness = getUniformLocation(toonProgram_, "uMaterialBrightness");
    toonUniforms_.useInstancing = getUniformLocation(toonProgram_, "uUseInstancing");
    toonUniforms_.pointLightPosition = getUniformLocation(toonProgram_, "uPointLightPosition");
    toonUniforms_.pointLightColor = getUniformLocation(toonProgram_, "uPointLightColor");
    toonUniforms_.pointLightIntensity = getUniformLocation(toonProgram_, "uPointLightIntensity");
    toonUniforms_.pointLightRadius = getUniformLocation(toonProgram_, "uPointLightRadius");
    toonUniforms_.useEmission = getUniformLocation(toonProgram_, "uUseEmission");
    toonUniforms_.diffuseTexture = getUniformLocation(toonProgram_, "uDiffuseTexture");
    toonUniforms_.shadowMap = getUniformLocation(toonProgram_, "uShadowMap");

    skyboxUniforms_.view = getUniformLocation(skyboxProgram_, "uView");
    skyboxUniforms_.projection = getUniformLocation(skyboxProgram_, "uProjection");
    skyboxUniforms_.skybox = getUniformLocation(skyboxProgram_, "uSkybox");

    shadowUniforms_.model = getUniformLocation(shadowProgram_, "uModel");
    shadowUniforms_.lightSpaceMatrix = getUniformLocation(shadowProgram_, "uLightSpaceMatrix");
    shadowUniforms_.useInstancing = getUniformLocation(shadowProgram_, "uUseInstancing");
}

GLint Renderer::getUniformLocation(GLuint program, const char* name) const
{
    return glGetUniformLocation(program, name);
}

void Renderer::setMat4(GLint location, const glm::mat4& value) const
{
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Renderer::setVec3(GLint location, const glm::vec3& value) const
{
    glUniform3fv(location, 1, glm::value_ptr(value));
}

void Renderer::setFloat(GLint location, float value) const
{
    glUniform1f(location, value);
}

void Renderer::setInt(GLint location, int value) const
{
    glUniform1i(location, value);
}
