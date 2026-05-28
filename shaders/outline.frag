#version 330 core

uniform vec3 uOutlineColor;

out vec4 fragColor;

void main()
{
    fragColor = vec4(uOutlineColor, 1.0);
}
