#version 330 core

in vec3 vWorldNormal;

uniform vec3 uBaseColor;
uniform vec3 uLightDirection;
uniform vec3 uAmbientColor;

out vec4 fragColor;

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDir = normalize(-uLightDirection);
    float ndotl = max(dot(normal, lightDir), 0.0);

    float shade = 0.45;
    if (ndotl > 0.75)
    {
        shade = 1.0;
    }
    else if (ndotl > 0.4)
    {
        shade = 0.75;
    }

    vec3 color = uBaseColor * shade + uAmbientColor;
    fragColor = vec4(color, 1.0);
}
