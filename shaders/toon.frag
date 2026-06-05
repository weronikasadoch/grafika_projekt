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

    float shade = 0.35; // dark
    if (ndotl > 0.75)
    {
        shade = 1.0; // bright
    }
    else if (ndotl > 0.35)
    {
        shade = 0.65; // middle
    }

    vec3 color = clamp(uBaseColor * shade + uAmbientColor, 0.0, 1.0);
    fragColor = vec4(color, 1.0);
}
