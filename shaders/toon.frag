#version 330 core

in vec3 vWorldNormal;
in vec3 vWorldPosition;
in vec4 vLightSpacePosition;
in vec2 vTexCoord;
in vec3 vMaterialColor;

uniform vec3 uBaseColor;
uniform vec3 uLightDirection;
uniform vec3 uAmbientColor;
uniform sampler2D uDiffuseTexture;
uniform sampler2D uShadowMap;
uniform int uUseToonShading;
uniform int uUseMaterialColor;
uniform int uUseDiffuseTexture;
uniform float uMaterialBrightness;
uniform vec3 uPointLightPosition;
uniform vec3 uPointLightColor;
uniform float uPointLightIntensity;
uniform float uPointLightRadius;
uniform int uUseEmission;

out vec4 fragColor;

float calculateShadow(vec3 normal, vec3 lightDir)
{
    vec3 projectedCoords = vLightSpacePosition.xyz / vLightSpacePosition.w;
    projectedCoords = projectedCoords * 0.5 + 0.5;
    if (projectedCoords.x < 0.0 || projectedCoords.x > 1.0 ||
        projectedCoords.y < 0.0 || projectedCoords.y > 1.0 ||
        projectedCoords.z > 1.0)
    {
        return 0.0;
    }

    float currentDepth = projectedCoords.z;
    float bias = max(0.0015 * (1.0 - dot(normal, lightDir)), 0.0005);
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float shadow = 0.0;
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

    vec3 surfaceColor = uUseMaterialColor == 1 ? vMaterialColor * uMaterialBrightness : uBaseColor;
    if (uUseDiffuseTexture == 1)
    {
        surfaceColor *= texture(uDiffuseTexture, vTexCoord).rgb;
    }

    vec3 litColor = surfaceColor * shade;
    litColor *= mix(1.0, 0.70, calculateShadow(normal, lightDir));

    vec3 pointVector = uPointLightPosition - vWorldPosition;
    float pointDistance = length(pointVector);
    vec3 pointDirection = pointDistance > 0.001 ? pointVector / pointDistance : normal;
    float pointNdotL = max(dot(normal, pointDirection), 0.0);
    float pointAttenuation = clamp(1.0 - pointDistance / uPointLightRadius, 0.0, 1.0);
    pointAttenuation *= pointAttenuation;
    litColor += surfaceColor * uPointLightColor * pointNdotL * pointAttenuation * uPointLightIntensity;
    litColor += uPointLightColor * pointAttenuation * 0.06;

    if (uUseEmission == 1)
    {
        litColor += uPointLightColor * 1.4;
    }

    vec3 color = clamp(litColor + uAmbientColor, 0.0, 1.0);
    fragColor = vec4(color, 1.0);
}
