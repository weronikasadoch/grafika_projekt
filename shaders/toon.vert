#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aMaterialColor;
layout (location = 4) in mat4 aInstanceModel;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;
uniform int uUseInstancing;

out vec3 vWorldNormal;
out vec3 vWorldPosition;
out vec4 vLightSpacePosition;
out vec2 vTexCoord;
out vec3 vMaterialColor;

void main()
{
    mat4 model = uUseInstancing == 1 ? aInstanceModel : uModel;
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vWorldNormal = normalize(normalMatrix * aNormal);
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vLightSpacePosition = uLightSpaceMatrix * worldPosition;
    vTexCoord = aTexCoord;
    vMaterialColor = aMaterialColor;
    gl_Position = uProjection * uView * worldPosition;
}
