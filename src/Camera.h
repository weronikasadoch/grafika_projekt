#pragma once

#include "glm.hpp"

class Camera
{
public:
    void moveForward(float distance);
    void moveRight(float distance);
    void rotateYaw(float degrees);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;

    const glm::vec3& getPosition() const;
    const glm::vec3& getFront() const;
    const glm::vec3& getUp() const;

private:
    void updateFront();

    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 4.0f);
    glm::vec3 front_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw_ = -90.0f;
};

namespace Core
{
	glm::mat4 createPerspectiveMatrix(float zNear = 0.1f, float zFar = 100.0f, float frustumScale = 1.f);

	// position - pozycja kamery
	// forward - wektor "do przodu" kamery (jednostkowy)
	// up - wektor "w gore" kamery (jednostkowy)
	// up i forward musza byc ortogonalne!
	glm::mat4 createViewMatrix(glm::vec3 position, glm::vec3 forward, glm::vec3 up);
}
