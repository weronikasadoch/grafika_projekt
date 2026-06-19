#include "Model.h"


#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace
{
    constexpr int kVertexStride = 11;

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

    std::map<std::string, std::array<float, 3>> loadMtlDiffuseColors(const std::string& path)
    {
        std::map<std::string, std::array<float, 3>> materials;
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
            }
            else if (command == "Kd" && !activeMaterial.empty())
            {
                std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
                stream >> color[0] >> color[1] >> color[2];
                materials[activeMaterial] = color;
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
    std::map<std::string, std::array<float, 3>> materials;
    std::array<float, 3> currentColor = {1.0f, 1.0f, 1.0f};
    const std::string directory = getDirectory(path);

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
            materials = loadMtlDiffuseColors(directory + materialFile);
        }
        else if (command == "usemtl")
        {
            std::string materialName;
            stream >> materialName;
            const auto material = materials.find(materialName);
            currentColor = material != materials.end() ? material->second : std::array<float, 3>{1.0f, 1.0f, 1.0f};
        }
        else if (command == "v")
        {
            std::array<float, 3> position = {};
            stream >> position[0] >> position[1] >> position[2];
            positions.push_back(position);
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

    if (vertices.empty() || indices.empty())
    {
        std::cerr << "OBJ model has no drawable faces: " << path << '\n';
        return false;
    }

    indexCount_ = static_cast<GLsizei>(indices.size());

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
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
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
}
