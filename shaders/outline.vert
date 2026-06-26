#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 4) in mat4 aInstanceModel;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uOutlineThickness;
uniform int uUseInstancing;

void main()
{
    mat4 model = uUseInstancing == 1 ? aInstanceModel : uModel;
    vec3 outlinedPosition = aPosition + normalize(aNormal) * uOutlineThickness;
    gl_Position = uProjection * uView * model * vec4(outlinedPosition, 1.0);
}
