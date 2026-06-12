#version 330 core

in vec3 vWorldNormal;
in vec4 vLightSpacePosition;
in vec2 vTexCoord;
in vec3 vMaterialColor;

uniform vec3 uBaseColor;
uniform vec3 uLightDirection;
uniform vec3 uAmbientColor;
uniform sampler2D uShadowMap;
uniform sampler2D uDiffuseTexture;
uniform int uReceiveShadow;
uniform int uUseToonShading;
uniform int uUseMaterialColor;
uniform int uUseDiffuseTexture;

out vec4 fragColor;

float calculateShadow()
{
    vec3 projectedCoords = vLightSpacePosition.xyz / vLightSpacePosition.w;
    projectedCoords = projectedCoords * 0.5 + 0.5;

    if (projectedCoords.z > 1.0)
    {
        return 0.0;
    }

    float currentDepth = projectedCoords.z;
    float bias = 0.006;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(uShadowMap, projectedCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDir = normalize(-uLightDirection);
    float ndotl = max(dot(normal, lightDir), 0.0);

    float shade = 0.25 + ndotl * 0.75;
    if (uUseToonShading == 1)
    {
        shade = 0.35; // dark
        if (ndotl > 0.75)
        {
            shade = 1.0; // bright
        }
        else if (ndotl > 0.35)
        {
            shade = 0.65; // middle
        }
    }

    float shadow = uReceiveShadow == 1 ? calculateShadow() : 0.0;
    vec3 surfaceColor = uUseMaterialColor == 1 ? vMaterialColor : uBaseColor;
    if (uUseDiffuseTexture == 1)
    {
        surfaceColor *= texture(uDiffuseTexture, vTexCoord).rgb;
    }

    vec3 litColor = surfaceColor * shade;
    litColor *= mix(1.0, 0.45, shadow);

    vec3 color = clamp(litColor + uAmbientColor, 0.0, 1.0);
    fragColor = vec4(color, 1.0);
}
