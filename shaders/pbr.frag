#version 330 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec4 vLightSpacePosition;
in vec2 vTexCoord;
in vec3 vMaterialColor;

uniform vec3 uBaseColor;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform sampler2D uShadowMap;
uniform sampler2D uDiffuseTexture;
uniform samplerCube uPointShadowMap;
uniform int uReceiveShadow;
uniform int uUseMaterialColor;
uniform int uUseDiffuseTexture;
uniform float uMaterialBrightness;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAo;
uniform int uUseFastPbr;
uniform vec3 uPointLightPosition;
uniform vec3 uPointLightColor;
uniform float uPointLightIntensity;
uniform float uPointLightRadius;
uniform float uPointLightFarPlane;
uniform int uUseEmission;

out vec4 fragColor;

const float PI = 3.14159265359;

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

float calculatePointShadow(vec3 pointVector)
{
    float currentDepth = length(pointVector);
    if (currentDepth > uPointLightFarPlane)
    {
        return 0.0;
    }

    float closestDepth = texture(uPointShadowMap, pointVector).r * uPointLightFarPlane;
    float bias = 0.04;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

float distributionGGX(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfway), 0.0);
    float ndoth2 = ndoth * ndoth;
    float denominator = (ndoth2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return a2 / max(denominator, 0.0001);
}

float geometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness)
{
    float ndotv = max(dot(normal, viewDir), 0.0);
    float ndotl = max(dot(normal, lightDir), 0.0);
    return geometrySchlickGGX(ndotv, roughness) * geometrySchlickGGX(ndotl, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x2 = x * x;
    return f0 + (1.0 - f0) * x2 * x2 * x;
}

vec3 calculatePbrLight(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 radiance, float metallic, float roughness)
{
    vec3 halfway = normalize(viewDir + lightDir);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float ndf = distributionGGX(normal, halfway, roughness);
    float geometry = geometrySmith(normal, viewDir, lightDir, roughness);
    vec3 fresnel = fresnelSchlick(max(dot(halfway, viewDir), 0.0), f0);

    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
    float ndotl = max(dot(normal, lightDir), 0.0);
    return (diffuseWeight * albedo / PI + specular) * radiance * ndotl;
}

void main()
{
    vec3 albedo = uUseMaterialColor == 1 ? vMaterialColor * uMaterialBrightness : uBaseColor;
    if (uUseDiffuseTexture == 1)
    {
        albedo *= texture(uDiffuseTexture, vTexCoord).rgb;
    }
    albedo = pow(clamp(albedo, 0.0, 1.0), vec3(2.2));

    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    vec3 normal = normalize(vWorldNormal);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);

    float directionalShadow = uReceiveShadow == 1 ? calculateShadow() : 0.0;
    if (uUseFastPbr == 1)
    {
        vec3 lightDir = normalize(-uLightDirection);
        float ndotl = max(dot(normal, lightDir), 0.0);
        vec3 halfway = normalize(viewDir + lightDir);
        float specularPower = mix(64.0, 4.0, roughness);
        float specular = pow(max(dot(normal, halfway), 0.0), specularPower) * (1.0 - roughness) * (1.0 - metallic);
        vec3 color = uAmbientColor * albedo * uAo;
        color += (albedo * ndotl + vec3(specular)) * uLightColor * (1.0 - directionalShadow);
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));
        fragColor = vec4(color, 1.0);
        return;
    }

    vec3 direct = calculatePbrLight(albedo, normal, viewDir, normalize(-uLightDirection), uLightColor * (1.0 - directionalShadow), metallic, roughness);

    vec3 point = vec3(0.0);
    vec3 pointVector = uPointLightPosition - vWorldPosition;
    float pointDistance = length(pointVector);
    if (pointDistance < uPointLightRadius)
    {
        vec3 pointDir = pointDistance > 0.001 ? pointVector / pointDistance : normal;
        float pointAttenuation = 1.0 - pointDistance / uPointLightRadius;
        pointAttenuation *= pointAttenuation;
        float pointVisibility = 1.0 - calculatePointShadow(vWorldPosition - uPointLightPosition);
        vec3 pointRadiance = uPointLightColor * uPointLightIntensity * pointAttenuation * pointVisibility;
        point = calculatePbrLight(albedo, normal, viewDir, pointDir, pointRadiance, metallic, roughness);
    }

    vec3 ambient = uAmbientColor * albedo * uAo;
    vec3 color = ambient + direct + point;
    if (uUseEmission == 1)
    {
        color += uPointLightColor * 1.2;
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
