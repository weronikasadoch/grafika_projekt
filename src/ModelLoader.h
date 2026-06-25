#pragma once
#include "Mesh.h" 
#include <string>
#include <vector>

struct aiNode;
struct aiScene;
struct aiMesh;

class AssimpModel {
public:
    std::vector<Mesh> meshes;
    std::string directory;

    bool loadModel(const std::string& path);
    void Draw(unsigned int shaderProgram) const;
    void destroy();

private:
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    unsigned int loadTextureFromFile(const char* path, const std::string& directory);
};