#include "Model.h"

#include <algorithm>

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
    constexpr int kVertexStride = 11;

    struct MaterialInfo
    {
        std::array<float, 3> kd = {1.0f, 1.0f, 1.0f};
        std::string mapKd;
    };

    struct ObjVertex
    {
        int position = -1;
        int texCoord = -1;
        int normal = -1;
    };

    std::string getDirectory(const std::string& path)
    {
        const size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos)
        {
            return "";
        }
        return path.substr(0, slash + 1);
    }

    ObjVertex parseObjVertex(const std::string& token)
    {
        ObjVertex vertex;
        std::stringstream stream(token);
        std::string part;

        if (std::getline(stream, part, '/') && !part.empty())
        {
            vertex.position = std::stoi(part) - 1;
        }
        if (std::getline(stream, part, '/') && !part.empty())
        {
            vertex.texCoord = std::stoi(part) - 1;
        }
        if (std::getline(stream, part, '/') && !part.empty())
        {
            vertex.normal = std::stoi(part) - 1;
        }

        return vertex;
    }

    std::map<std::string, MaterialInfo> loadMtlMaterials(const std::string& path)
    {
        std::map<std::string, MaterialInfo> materials;
        std::ifstream file(path);
        if (!file)
        {
            return materials;
        }

        std::string line;
        std::string activeMaterial;
        while (std::getline(file, line))
        {
            std::stringstream stream(line);
            std::string command;
            stream >> command;

            if (command == "newmtl")
            {
                stream >> activeMaterial;
                materials[activeMaterial] = MaterialInfo{};
            }
            else if (command == "Kd" && !activeMaterial.empty())
            {
                std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
                stream >> color[0] >> color[1] >> color[2];
                materials[activeMaterial].kd = color;
            }
            else if ((command == "map_Kd" || command == "map_kd") && !activeMaterial.empty())
            {
                std::string token;
                std::string lastToken;
                while (stream >> token)
                {
                    lastToken = token;
                }
                if (!lastToken.empty())
                {
                    materials[activeMaterial].mapKd = lastToken;
                }
            }
        }

        return materials;
    }

    void uploadTexture(GLuint& texture, int width, int height, GLenum format, const unsigned char* pixels)
    {
        if (texture == 0)
        {
            glGenTextures(1, &texture);
        }

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, format == GL_RGBA ? GL_RGBA8 : GL_RGB8, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint loadTextureId(const std::string& path)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            std::cerr << "Failed to load image texture: " << path << '\n';
            stbi_image_free(pixels);
            return 0;
        }

        GLuint texture = 0;
        uploadTexture(texture, width, height, GL_RGBA, pixels);
        stbi_image_free(pixels);
        return texture;
    }

    GLuint createWhiteTexture()
    {
        const std::array<unsigned char, 4> pixel = {255, 255, 255, 255};
        GLuint texture = 0;
        uploadTexture(texture, 1, 1, GL_RGBA, pixel.data());
        return texture;
    }
}

bool Texture::loadImage(const std::string& path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        std::cerr << "Failed to load image texture: " << path << '\n';
        stbi_image_free(pixels);
        return false;
    }

    uploadTexture(texture_, width, height, GL_RGBA, pixels);
    stbi_image_free(pixels);
    return true;
}

bool Texture::loadImageData(const unsigned char* data, int size)
{
    if (data == nullptr || size <= 0)
    {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        std::cerr << "Failed to load image texture from GLB data\n";
        stbi_image_free(pixels);
        return false;
    }

    uploadTexture(texture_, width, height, GL_RGBA, pixels);
    stbi_image_free(pixels);
    return true;
}

bool Texture::loadPPM(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }

    std::string header;
    int width = 0;
    int height = 0;
    int maxValue = 0;
    file >> header >> width >> height >> maxValue;
    file.get();

    if (header != "P6" || width <= 0 || height <= 0 || maxValue != 255)
    {
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 3));
    file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!file)
    {
        return false;
    }

    uploadTexture(texture_, width, height, GL_RGB, pixels.data());
    return true;
}

void Texture::createSolidColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    const std::array<unsigned char, 4> pixel = {r, g, b, a};
    uploadTexture(texture_, 1, 1, GL_RGBA, pixel.data());
}

void Texture::bind(GLenum textureUnit) const
{
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

void Texture::destroy()
{
    glDeleteTextures(1, &texture_);
    texture_ = 0;
}

bool Model::loadFromObj(const std::string& path)
{
    destroy();
    minBounds_ = glm::vec3(0.0f);
    maxBounds_ = glm::vec3(0.0f);

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Failed to load OBJ model: " << path << '\n';
        return false;
    }

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> texCoords;
    std::vector<std::array<float, 3>> normals;
    std::map<std::string, MaterialInfo> materials;
    std::map<std::string, GLuint> loadedMaterialTextures;
    std::array<float, 3> currentColor = {1.0f, 1.0f, 1.0f};
    GLuint currentTexture = 0;
    bool currentHasTexture = false;
    bool hasActiveDrawRange = false;
    GLsizei activeRangeStart = 0;
    GLuint activeRangeTexture = 0;
    bool activeRangeHasTexture = false;
    const std::string directory = getDirectory(path);
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    auto finishActiveDrawRange = [&]()
    {
        if (!hasActiveDrawRange)
        {
            return;
        }

        const GLsizei end = static_cast<GLsizei>(indices.size());
        if (end > activeRangeStart)
        {
            drawRanges_.push_back({activeRangeStart, end - activeRangeStart, activeRangeTexture, activeRangeHasTexture});
        }
    };

    auto startDrawRange = [&]()
    {
        finishActiveDrawRange();
        hasActiveDrawRange = true;
        activeRangeStart = static_cast<GLsizei>(indices.size());
        activeRangeTexture = currentTexture;
        activeRangeHasTexture = currentHasTexture;
    };

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream stream(line);
        std::string command;
        stream >> command;

        if (command == "mtllib")
        {
            std::string materialFile;
            stream >> materialFile;
            materials = loadMtlMaterials(directory + materialFile);
        }
        else if (command == "usemtl")
        {
            std::string materialName;
            stream >> materialName;
            const auto material = materials.find(materialName);
            if (material != materials.end())
            {
                currentColor = material->second.kd;
                currentTexture = 0;
                currentHasTexture = false;
                if (!material->second.mapKd.empty())
                {
                    const std::string texturePath = directory + material->second.mapKd;
                    auto loadedTexture = loadedMaterialTextures.find(texturePath);
                    if (loadedTexture == loadedMaterialTextures.end())
                    {
                        const GLuint texture = loadTextureId(texturePath);
                        loadedTexture = loadedMaterialTextures.emplace(texturePath, texture).first;
                        if (texture != 0)
                        {
                            materialTextures_.push_back(texture);
                        }
                    }

                    currentTexture = loadedTexture->second;
                    currentHasTexture = currentTexture != 0;
                    if (!hasDiffuseTexture_ && currentHasTexture)
                    {
                        hasDiffuseTexture_ = diffuseTexture_.loadImage(texturePath);
                    }
                }
            }
            else
            {
                currentColor = {1.0f, 1.0f, 1.0f};
                currentTexture = 0;
                currentHasTexture = false;
            }

            startDrawRange();
        }
        else if (command == "v")
        {
            std::array<float, 3> position = {};
            stream >> position[0] >> position[1] >> position[2];
            positions.push_back(position);
            minBounds.x = std::min(minBounds.x, position[0]);
            minBounds.y = std::min(minBounds.y, position[1]);
            minBounds.z = std::min(minBounds.z, position[2]);
            maxBounds.x = std::max(maxBounds.x, position[0]);
            maxBounds.y = std::max(maxBounds.y, position[1]);
            maxBounds.z = std::max(maxBounds.z, position[2]);
        }
        else if (command == "vt")
        {
            std::array<float, 2> texCoord = {};
            stream >> texCoord[0] >> texCoord[1];
            texCoords.push_back(texCoord);
        }
        else if (command == "vn")
        {
            std::array<float, 3> normal = {};
            stream >> normal[0] >> normal[1] >> normal[2];
            normals.push_back(normal);
        }
        else if (command == "f")
        {
            if (!hasActiveDrawRange)
            {
                startDrawRange();
            }

            std::vector<ObjVertex> face;
            std::string token;
            while (stream >> token)
            {
                face.push_back(parseObjVertex(token));
            }

            if (face.size() < 3)
            {
                continue;
            }

            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                const ObjVertex triangle[] = {face[0], face[i], face[i + 1]};
                for (const ObjVertex& vertex : triangle)
                {
                    if (vertex.position < 0 || static_cast<size_t>(vertex.position) >= positions.size())
                    {
                        continue;
                    }

                    const auto& position = positions[static_cast<size_t>(vertex.position)];
                    const std::array<float, 3> normal = vertex.normal >= 0 && static_cast<size_t>(vertex.normal) < normals.size()
                        ? normals[static_cast<size_t>(vertex.normal)]
                        : std::array<float, 3>{0.0f, 1.0f, 0.0f};
                    const std::array<float, 2> texCoord = vertex.texCoord >= 0 && static_cast<size_t>(vertex.texCoord) < texCoords.size()
                        ? texCoords[static_cast<size_t>(vertex.texCoord)]
                        : std::array<float, 2>{0.0f, 0.0f};

                    vertices.push_back(position[0]);
                    vertices.push_back(position[1]);
                    vertices.push_back(position[2]);
                    vertices.push_back(normal[0]);
                    vertices.push_back(normal[1]);
                    vertices.push_back(normal[2]);
                    vertices.push_back(texCoord[0]);
                    vertices.push_back(texCoord[1]);
                    vertices.push_back(currentColor[0]);
                    vertices.push_back(currentColor[1]);
                    vertices.push_back(currentColor[2]);
                    indices.push_back(static_cast<unsigned int>(indices.size()));
                }
            }
        }
    }
    finishActiveDrawRange();

    if (vertices.empty() || indices.empty())
    {
        std::cerr << "OBJ model has no drawable faces: " << path << '\n';
        return false;
    }

    indexCount_ = static_cast<GLsizei>(indices.size());
    minBounds_ = minBounds;
    maxBounds_ = maxBounds;
    if (!drawRanges_.empty() && whiteTexture_ == 0)
    {
        whiteTexture_ = createWhiteTexture();
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, kVertexStride * sizeof(float), reinterpret_cast<void*>(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    return true;
}

void Model::draw() const
{
    glBindVertexArray(vao_);
    if (drawRanges_.empty())
    {
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        for (const DrawRange& range : drawRanges_)
        {
            glBindTexture(GL_TEXTURE_2D, range.hasTexture ? range.texture : whiteTexture_);
            glDrawElements(
                GL_TRIANGLES,
                range.indexCount,
                GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(static_cast<std::size_t>(range.indexOffset) * sizeof(unsigned int))
            );
        }
    }
    glBindVertexArray(0);
}

void Model::drawInstanced(GLuint instanceBuffer, GLsizei instanceCount) const
{
    if (instanceCount <= 0)
    {
        return;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);

    constexpr GLsizei matrixStride = 16 * sizeof(float);
    for (int column = 0; column < 4; ++column)
    {
        const GLuint attribute = static_cast<GLuint>(4 + column);
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(
            attribute,
            4,
            GL_FLOAT,
            GL_FALSE,
            matrixStride,
            reinterpret_cast<void*>(static_cast<std::size_t>(column * 4) * sizeof(float))
        );
        glVertexAttribDivisor(attribute, 1);
    }

    glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr, instanceCount);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Model::destroy()
{
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
    vbo_ = 0;
    ebo_ = 0;
    indexCount_ = 0;
    minBounds_ = glm::vec3(0.0f);
    maxBounds_ = glm::vec3(0.0f);

    if (hasDiffuseTexture_)
    {
        diffuseTexture_.destroy();
        hasDiffuseTexture_ = false;
    }
    for (GLuint texture : materialTextures_)
    {
        glDeleteTextures(1, &texture);
    }
    materialTextures_.clear();
    drawRanges_.clear();
    glDeleteTextures(1, &whiteTexture_);
    whiteTexture_ = 0;
}
