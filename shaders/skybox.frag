#version 330 core

in vec3 vDirection;

uniform samplerCube uSkybox;

out vec4 fragColor;

void main()
{
    vec3 direction = normalize(vDirection);
    vec3 color = texture(uSkybox, direction).rgb;
    fragColor = vec4(color, 1.0);
}
