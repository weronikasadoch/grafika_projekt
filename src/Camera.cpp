#include "Camera.h"

#include <gtc/matrix_transform.hpp>

#include <cmath>

void Camera::moveForward(float distance)
{
    position_ += front_ * distance;
}

void Camera::moveRight(float distance)
{
    const glm::vec3 right = glm::normalize(glm::cross(front_, up_));
    position_ += right * distance;
}

void Camera::rotateYaw(float degrees)
{
    yaw_ += degrees;
    updateFront();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const
{
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

const glm::vec3& Camera::getPosition() const
{
    return position_;
}

const glm::vec3& Camera::getFront() const
{
    return front_;
}

const glm::vec3& Camera::getUp() const
{
    return up_;
}

void Camera::updateFront()
{
    front_ = glm::normalize(glm::vec3(
        std::cos(glm::radians(yaw_)),
        0.0f,
        std::sin(glm::radians(yaw_))
    ));
}

glm::mat4 Core::createPerspectiveMatrix(float zNear, float zFar, float frustumScale)
{
	glm::mat4 perspective;
    perspective[0][0] = 1.f;
	perspective[1][1] = frustumScale;
	perspective[2][2] = (zFar + zNear) / (zNear - zFar);
	perspective[3][2] = (2 * zFar * zNear) / (zNear - zFar);
	perspective[2][3] = -1;
	perspective[3][3] = 0;

	return perspective;
}

glm::mat4 Core::createViewMatrix( glm::vec3 position, glm::vec3 forward, glm::vec3 up )
{
	glm::vec3 side = glm::cross(forward, up);

	// Trzeba pamietac o minusie przy ustawianiu osi Z kamery.
	// Wynika to z tego, ze standardowa macierz perspektywiczna zaklada, ze "z przodu" jest ujemna (a nie dodatnia) czesc osi Z.
	glm::mat4 cameraRotation;
	cameraRotation[0][0] = side.x; cameraRotation[1][0] = side.y; cameraRotation[2][0] = side.z;
	cameraRotation[0][1] = up.x; cameraRotation[1][1] = up.y; cameraRotation[2][1] = up.z;
	cameraRotation[0][2] = -forward.x; cameraRotation[1][2] = -forward.y; cameraRotation[2][2] = -forward.z;

	glm::mat4 cameraTranslation;
	cameraTranslation[3] = glm::vec4(-position, 1.0f);

	return cameraRotation * cameraTranslation;
}
