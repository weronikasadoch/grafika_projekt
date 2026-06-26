#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 4) in mat4 aInstanceModel;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform int uUseInstancing;

void main()
{
    mat4 model = uUseInstancing == 1 ? aInstanceModel : uModel;
    gl_Position = uLightSpaceMatrix * model * vec4(aPosition, 1.0);
}
