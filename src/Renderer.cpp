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
    constexpr int kShadowMapSize = 2048;
    constexpr int kPointShadowMapSize = 512;
    constexpr int kJellyfishCount = 10;
    constexpr int kCoralModelCount = 10;
    constexpr int kCoralPlacementCount = 32;
    constexpr int kVisibleCoralInstanceCount = 1;
    constexpr int kCoralShadowCasterCount = kVisibleCoralInstanceCount;
    constexpr int kLightJellyfishIndex = 2;
    constexpr float kPointLightNearPlane = 0.05f;
    constexpr float kPointLightFarPlane = 8.0f;
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
        {6, -7.0f, -0.88f,  5.8f,  18.0f, 1.0f},
        {6, -6.4f, -0.88f,  5.4f, -28.0f, 1.0f},
        {7, -5.7f, -0.90f,  5.9f,  52.0f, 1.0f},
        {8, -6.9f, -0.88f,  6.6f,  95.0f, 1.0f},
        {9, -6.1f, -0.88f,  6.5f, -70.0f, 1.0f},
        {5, -5.3f, -0.90f,  6.4f,  34.0f, 1.0f},
        {6, -7.4f, -0.88f,  6.2f, 142.0f, 1.0f},
        {7, -6.7f, -0.88f,  7.1f, -18.0f, 1.0f},
        {8, -5.9f, -0.88f,  7.2f,  63.0f, 1.0f},
        {9, -5.1f, -0.88f,  7.0f, -92.0f, 1.0f},
        {5, -7.8f, -0.88f,  7.0f, 126.0f, 0.24f},
        {6, -7.2f, -0.88f,  7.7f, -38.0f, 0.36f},
        {7, -6.3f, -0.89f,  7.8f,  76.0f, 0.31f},
        {8, -5.4f, -0.88f,  7.8f, -120.0f, 0.38f},
        {9, -4.7f, -0.88f,  7.5f,  22.0f, 0.20f},
        {5, -8.1f, -0.90f,  6.2f,  48.0f, 0.24f},
        {6, -7.6f, -0.88f,  5.3f, -82.0f, 0.36f},
        {7, -6.8f, -0.88f,  4.8f, 114.0f, 0.31f},
        {8, -5.8f, -0.88f,  4.9f, -12.0f, 0.38f},
        {9, -5.0f, -0.88f,  5.4f, 154.0f, 0.20f},
        {0, -8.9f, -0.88f,  0.7f, -55.0f, 0.18f},
        {1, -6.9f, -0.88f,  8.1f,  86.0f, 0.20f},
        {2, -3.5f, -0.90f,  2.2f, -144.0f, 0.16f},
        {3, -1.4f, -0.88f,  2.9f,  31.0f, 0.18f},
        {4,  1.9f, -0.88f,  2.2f, -37.0f, 0.19f},
        {5,  6.2f, -0.90f, -3.6f,  71.0f, 0.22f},
        {6,  7.1f, -0.88f, -0.2f, -101.0f, 0.34f},
        {7, -1.4f, -0.88f, -8.8f,  12.0f, 0.30f},
        {8,  0.4f, -0.88f,  0.8f, 168.0f, 0.36f},
        {9, -6.4f, -0.88f, -5.7f, -78.0f, 0.18f},
        {3,  8.9f, -0.88f,  8.2f,  42.0f, 0.19f},
        {4, -9.3f, -0.88f, -8.5f, -24.0f, 0.20f}
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
    char pointShadowVertexShaderPath[] = "shaders/point_shadow_depth.vert";
    char pointShadowFragmentShaderPath[] = "shaders/point_shadow_depth.frag";
    toonProgram_ = shaderLoader.CreateProgram(toonVertexShaderPath, toonFragmentShaderPath);
    pbrProgram_ = shaderLoader.CreateProgram(pbrVertexShaderPath, pbrFragmentShaderPath);
    outlineProgram_ = shaderLoader.CreateProgram(outlineVertexShaderPath, outlineFragmentShaderPath);
    skyboxProgram_ = shaderLoader.CreateProgram(skyboxVertexShaderPath, skyboxFragmentShaderPath);
    shadowProgram_ = shaderLoader.CreateProgram(shadowVertexShaderPath, shadowFragmentShaderPath);
    pointShadowProgram_ = shaderLoader.CreateProgram(pointShadowVertexShaderPath, pointShadowFragmentShaderPath);
    const bool sandLoaded = sandModel_.loadFromObj("assets/models/scene/sand.obj");
    const bool spongebobLoaded = spongebobModel_.loadFromObj("assets/models/houses/spongebob/spongebob_house_1.obj");
    const bool patrickLoaded = patrickModel_.loadFromObj("assets/models/houses/patrick/patrick_house_1.obj");
    const bool squidwardLoaded = squidwardModel_.loadFromObj("assets/models/houses/squidward/squidward_house_1.obj");
    const bool characterLoaded = characterModel_.loadFromObj("assets/models/Spongebob_model/spongebob_model.obj");
    const bool jellyfishLoaded = jellyfishModel_.loadFromObj("assets/models/Jellyfish_model/jellyfish_model.obj");
    const char* coralModelPaths[kCoralModelCount] = {
        "assets/models/coral_rock/coral_1.obj",
        "assets/models/coral_rock/coral_2.obj",
        "assets/models/coral_rock/coral_3.obj",
        "assets/models/coral_rock/coral_4.obj",
        "assets/models/coral_rock/coral_5.obj",
        "assets/models/coral_rock/coral_6.obj",
        "assets/models/coral_rock/coral_7.obj",
        "assets/models/coral_rock/coral_8.obj",
        "assets/models/coral_rock/coral_9.obj",
        "assets/models/coral_rock/coral_10.obj"
    };
    bool coralsLoaded = true;
    for (int i = 0; i < kCoralModelCount; ++i)
    {
        coralsLoaded = coralModels_[static_cast<std::size_t>(i)].loadFromObj(coralModelPaths[i]) && coralsLoaded;
    }
    const bool spongebobTextureLoaded = spongebobTexture_.loadPPM("assets/models/Spongebob_model/spongebob.ppm");
    spongebobFallbackTexture_.createSolidColor(255, 214, 54);
    const bool skyboxResourcesCreated = createSkyboxResources();
    const bool shadowResourcesCreated = createShadowResources();

    return toonProgram_ != 0 &&
        pbrProgram_ != 0 &&
        outlineProgram_ != 0 &&
        skyboxProgram_ != 0 &&
        shadowProgram_ != 0 &&
        pointShadowProgram_ != 0 &&
        sandLoaded &&
        spongebobLoaded &&
        patrickLoaded &&
        squidwardLoaded &&
        characterLoaded &&
        jellyfishLoaded &&
        coralsLoaded &&
        spongebobTextureLoaded &&
        skyboxResourcesCreated &&
        shadowResourcesCreated;
}

void Renderer::render(const Scene& scene)
{
    const glm::mat4 view = scene.getCamera().getViewMatrix();
    const glm::mat4 projection = scene.getCamera().getProjectionMatrix(scene.getAspectRatio());
    const glm::mat4 lightSpace = createLightSpaceMatrix();
    const int jellyfishCount = std::clamp(scene.getJellyfishCount(), 0, kJellyfishCount);

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
    if (jellyfishCount > kLightJellyfishIndex)
    {
        pointLightPosition_ = glm::vec3(createJellyfishTransform(kLightJellyfishIndex, scene.getJellyfishAnimationTime(kLightJellyfishIndex)) * glm::vec4(0.0f, 0.45f, 0.0f, 1.0f));
        pointLightIntensity_ = 0.9f;
    }
    else
    {
        pointLightIntensity_ = 0.0f;
    }

    //const glm::mat4 characterModel = glm::scale(
      //  glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -1.0f)),
        //glm::vec3(0.45f)
    //);

    renderShadowMap(lightSpace, spongebobModel, patrickModel, squidwardModel, characterModel, scene, jellyfishCount);
    renderPointShadowMap(spongebobModel, patrickModel, squidwardModel, characterModel, scene, jellyfishCount);
    

    glViewport(0, 0, static_cast<GLsizei>(scene.getFramebufferWidth()), static_cast<GLsizei>(scene.getFramebufferHeight()));
    glClearColor(0.01f, 0.12f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSkybox(view, projection);

    const float outlineThickness = scene.getOutlineThickness();
    const float smallModelOutlineThickness = outlineThickness * 0.32f;

    renderModel(sandModel_, sand, view, projection, lightSpace, glm::vec3(0.86f, 0.68f, 0.38f), 0.0f, 0.92f, true, false, nullptr, true, false, glm::vec3(0.04f, 0.12f, 0.13f), 0.0f, 0.96f, true);
    for (int i = 0; i < kVisibleCoralInstanceCount; ++i)
    {
        const int modelIndex = kCoralPlacements[i].modelIndex;
        renderModel(coralModels_[static_cast<std::size_t>(modelIndex)], createCoralTransform(i, scene), view, projection, lightSpace, glm::vec3(0.95f, 0.25f, 0.48f), 0.0f, 1.45f, true, false, nullptr, true, false, glm::vec3(0.16f, 0.28f, 0.31f), 0.0f, 0.72f);
    }
    renderModel(spongebobModel_, spongebobModel, view, projection, lightSpace, glm::vec3(1.0f, 0.72f, 0.20f), outlineThickness, 2.6f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.58f);
    renderModel(patrickModel_, patrickModel, view, projection, lightSpace, glm::vec3(0.76f, 0.48f, 0.38f), outlineThickness, 2.6f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.88f);
    renderModel(squidwardModel_, squidwardModel, view, projection, lightSpace, glm::vec3(0.48f, 0.66f, 0.70f), outlineThickness, 4.4f, true, scene.isToonShadingEnabled(), nullptr, true, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.15f, 0.42f);
    renderModel(characterModel_, characterModel, view, projection, lightSpace, glm::vec3(1.0f), smallModelOutlineThickness, 1.0f, true, scene.isToonShadingEnabled(), &spongebobTexture_, false, false, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, 0.62f);
    for (int i = 0; i < jellyfishCount; ++i)
    {
        const bool isLightSource = i == kLightJellyfishIndex;
        const glm::vec3 jellyfishColor = isLightSource ? glm::vec3(0.45f, 0.95f, 1.0f) : glm::vec3(1.0f, 0.42f, 0.78f);
        renderModel(jellyfishModel_, createJellyfishTransform(i, scene.getJellyfishAnimationTime(i)), view, projection, lightSpace, jellyfishColor, smallModelOutlineThickness, isLightSource ? 2.2f : 1.6f, true, scene.isToonShadingEnabled(), nullptr, false, isLightSource, glm::vec3(0.18f, 0.30f, 0.34f), 0.0f, isLightSource ? 0.18f : 0.35f);
    }
}

void Renderer::shutdown()
{
    deleteSkyboxResources();
    deleteShadowResources();
    spongebobTexture_.destroy();
    spongebobFallbackTexture_.destroy();
    jellyfishModel_.destroy();
    for (Model& coralModel : coralModels_)
    {
        coralModel.destroy();
    }
    squidwardModel_.destroy();
    patrickModel_.destroy();
    spongebobModel_.destroy();
    sandModel_.destroy();
    characterModel_.destroy();
    glDeleteProgram(shadowProgram_);
    glDeleteProgram(pointShadowProgram_);
    glDeleteProgram(skyboxProgram_);
    glDeleteProgram(outlineProgram_);
    glDeleteProgram(pbrProgram_);
    glDeleteProgram(toonProgram_);
    shadowProgram_ = 0;
    pointShadowProgram_ = 0;
    skyboxProgram_ = 0;
    outlineProgram_ = 0;
    pbrProgram_ = 0;
    toonProgram_ = 0;
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
    std::array<unsigned char, size * size * channels> pixels = {};
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

                glm::vec3 deepColor(0.005f, 0.08f, 0.13f);
                glm::vec3 midColor(0.02f, 0.26f, 0.34f);
                glm::vec3 surfaceColor(0.22f, 0.72f, 0.78f);
                glm::vec3 color = glm::mix(deepColor, midColor, height);
                color = glm::mix(color, surfaceColor, surfaceLight * 0.55f);
                color += glm::vec3(0.05f, 0.18f, 0.16f) * caustics;

                if (!flowerMasks.empty())
                {
                    static constexpr float panelCenters[] = {0.22f, 0.34f, 0.46f, 0.58f, 0.70f, 0.82f};
                    static constexpr glm::vec3 panelColors[] = {
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

void Renderer::renderShadowMap(const glm::mat4& lightSpace, const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const
{
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shadowProgram_);
    renderModelShadowCaster(spongebobModel_, lightSpace, spongebobTransform);
    renderModelShadowCaster(patrickModel_, lightSpace, patrickTransform);
    renderModelShadowCaster(squidwardModel_, lightSpace, squidwardTransform);
    renderModelShadowCaster(characterModel_, lightSpace, characterTransform);
    for (int i = 0; i < kCoralShadowCasterCount; ++i)
    {
        const int modelIndex = kCoralPlacements[i].modelIndex;
        renderModelShadowCaster(coralModels_[static_cast<std::size_t>(modelIndex)], lightSpace, createCoralTransform(i, scene));
    }
    for (int i = 0; i < jellyfishCount; ++i)
    {
        renderModelShadowCaster(jellyfishModel_, lightSpace, createJellyfishTransform(i, scene.getJellyfishAnimationTime(i)));
    }
    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderPointShadowMap(const glm::mat4& spongebobTransform, const glm::mat4& patrickTransform, const glm::mat4& squidwardTransform, const glm::mat4& characterTransform, const Scene& scene, int jellyfishCount) const
{
    if (jellyfishCount <= kLightJellyfishIndex)
    {
        return;
    }

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
        for (int i = 0; i < jellyfishCount; ++i)
        {
            if (i == kLightJellyfishIndex)
            {
                continue;
            }
            renderPointShadowCaster(jellyfishModel_, lightSpace, createJellyfishTransform(i, scene.getJellyfishAnimationTime(i)));
        }
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

glm::mat4 Renderer::createCoralTransform(int index, const Scene& scene) const
{
    const DecorationPlacement& placement = kCoralPlacements[index % kCoralPlacementCount];
    const float scale = placement.scale;
    const float sandY = scene.getSandHeight(placement.x, placement.z);
    const float y = sandY - kCoralModelMinY[placement.modelIndex] * scale;

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(placement.x, y, placement.z));
    model = glm::rotate(model, glm::radians(placement.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale));
    return model;
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
