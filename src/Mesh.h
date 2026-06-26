#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <glm.hpp>
#include <string>
#include <vector>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct TextureInfo {
    unsigned int id;
    std::string type;
    std::string path;
    std::string color;
};

class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    TextureInfo               texture;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, TextureInfo texture) {
        this->vertices = vertices;
        this->indices = indices;
        this->texture = texture;
        setupMesh();
    }

    void Draw(unsigned int shaderProgram) const {
        if (texture.id != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture.id);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void DrawInstanced(GLuint instanceBuffer, GLsizei instanceCount) const {
        if (instanceCount <= 0) return;


        glBindVertexArray(VAO);

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

        if (texture.id != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture.id);
        }

        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0, instanceCount);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        for (int column = 0; column < 4; ++column)
        {
            const GLuint attribute = static_cast<GLuint>(4 + column);
            glVertexAttribDivisor(attribute, 0);
            glDisableVertexAttribArray(attribute);
        }

        glBindVertexArray(0);
    }

    void destroy() {
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        if (texture.id != 0) {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
        }
        VAO = 0; VBO = 0; EBO = 0;
    }

private:
    unsigned int VAO, VBO, EBO;
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);


        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
};