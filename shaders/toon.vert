#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

out vec3 vWorldNormal;
out vec4 vLightSpacePosition;

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vLightSpacePosition = uLightSpaceMatrix * worldPosition;
    gl_Position = uProjection * uView * worldPosition;
}
