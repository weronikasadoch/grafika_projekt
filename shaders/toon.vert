#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aMaterialColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

out vec3 vWorldNormal;
out vec4 vLightSpacePosition;
out vec2 vTexCoord;
out vec3 vMaterialColor;

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    vWorldNormal = normalize(normalMatrix * aNormal);
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vLightSpacePosition = uLightSpaceMatrix * worldPosition;
    vTexCoord = aTexCoord;
    vMaterialColor = aMaterialColor;
    gl_Position = uProjection * uView * worldPosition;
}
