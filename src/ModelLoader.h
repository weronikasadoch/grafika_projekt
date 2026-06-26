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
    void DrawInstanced(GLuint instanceBuffer, GLsizei instanceCount) const;
    void destroy();
    bool isLoaded() const { return !meshes.empty(); }
    glm::vec3 minBounds() const { return minBounds_; }
    glm::vec3 maxBounds() const { return maxBounds_; }

private:
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    unsigned int loadTextureFromFile(const char* path, const std::string& directory);

    glm::vec3 minBounds_ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 maxBounds_ = glm::vec3(std::numeric_limits<float>::lowest());
};