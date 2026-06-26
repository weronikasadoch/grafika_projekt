#include "ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>
#include <iostream>
#include <limits>

bool AssimpModel::loadModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals | 
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_FindInvalidData |
        aiProcess_GenUVCoords |       
        aiProcess_TransformUVCoords
    );
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "BŁĄD ASSIMP: " << importer.GetErrorString() << std::endl;
        return false;
    }
    directory = path.substr(0, path.find_last_of('/'));

    // Reset bounds
    minBounds_ = glm::vec3(std::numeric_limits<float>::max());
    maxBounds_ = glm::vec3(std::numeric_limits<float>::lowest());

    processNode(scene->mRootNode, scene);
    return true;
}

void AssimpModel::Draw(unsigned int shaderProgram) const {
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shaderProgram);
}

void AssimpModel::DrawInstanced(GLuint instanceBuffer, GLsizei instanceCount) const {
    for (unsigned int i = 0; i < meshes.size(); ++i)
        meshes[i].DrawInstanced(instanceBuffer, instanceCount);
}

void AssimpModel::processNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh AssimpModel::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    TextureInfo texture;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0]) {
            vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    if (!vertices.empty()) {
        glm::vec3 meshMin(std::numeric_limits<float>::max());
        glm::vec3 meshMax(std::numeric_limits<float>::lowest());
        for (const Vertex& v : vertices) {
            meshMin = glm::min(meshMin, v.Position);
            meshMax = glm::max(meshMax, v.Position);
        }
        minBounds_ = glm::min(minBounds_, meshMin);
        maxBounds_ = glm::max(maxBounds_, meshMax);
    }

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString str;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &str);

            texture.id = loadTextureFromFile(str.C_Str(), directory);
            texture.type = "texture_diffuse";
            texture.path = str.C_Str();
        }
        else {
            aiColor4D diffuseColor(0.8f, 0.8f, 0.8f, 1.0f); 
            aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuseColor);
            unsigned int colorTextureID;
            glGenTextures(1, &colorTextureID);
            glBindTexture(GL_TEXTURE_2D, colorTextureID);

            unsigned char r = static_cast<unsigned char>(diffuseColor.r * 255.0f);
            unsigned char g = static_cast<unsigned char>(diffuseColor.g * 255.0f);
            unsigned char b = static_cast<unsigned char>(diffuseColor.b * 255.0f);
            unsigned char a = static_cast<unsigned char>(diffuseColor.a * 255.0f);
            unsigned char pixelData[] = { r, g, b, a };

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            texture.id = colorTextureID;
            texture.type = "texture_diffuse";
            texture.path = "embedded_material_color";
        }
    }

    return Mesh(vertices, indices, texture);
}

unsigned int AssimpModel::loadTextureFromFile(const char* path, const std::string& directory) {

    std::string cleanPath = path;

    cleanPath.erase(cleanPath.find_last_not_of(" \n\r\t") + 1);
    cleanPath.erase(0, cleanPath.find_first_not_of(" \n\r\t"));

    std::string filename = directory + '/' + cleanPath;
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum internalFormat = GL_RGB;
        GLenum dataFormat = GL_RGB;

        if (nrComponents == 1) {
            internalFormat = GL_RED;
            dataFormat = GL_RED;
        }
        else if (nrComponents == 3) {
            internalFormat = GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4) {
            internalFormat = GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 
        GLint prevUnpack = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);


        glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        std::cout << "Assimp: Sukces! Zaladowano teksture: " << filename << std::endl;
    }
    else {
        std::cerr << "Assimp: BLAD! Nie udalo sie znalezc pliku: [" << filename << "]" << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void AssimpModel::destroy() {
    for (unsigned int i = 0; i < meshes.size(); i++) {
        meshes[i].destroy();
    }
    meshes.clear();
}