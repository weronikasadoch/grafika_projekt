#version 330 core

in vec3 vWorldPosition;

uniform vec3 uPointLightPosition;
uniform float uFarPlane;

void main()
{
    gl_FragDepth = length(vWorldPosition - uPointLightPosition) / uFarPlane;
}
