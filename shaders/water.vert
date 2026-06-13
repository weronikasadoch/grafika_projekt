#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out float vWave;

void main()
{
    vec3 localPosition = aPosition;
    float waveA = sin(localPosition.x * 0.045 + uTime * 0.85);
    float waveB = cos(localPosition.z * 0.060 + localPosition.x * 0.018 + uTime * 0.55);
    float waveC = sin((localPosition.x + localPosition.z) * 0.030 - uTime * 0.95);
    float ripple = sin(localPosition.x * 0.22 + localPosition.z * 0.11 + uTime * 1.7);
    localPosition.y += waveA * 2.2 + waveB * 1.45 + waveC * 0.95 + ripple * 0.28;

    float dx = 0.045 * cos(localPosition.x * 0.045 + uTime * 0.85) * 2.2
        - 0.018 * sin(localPosition.z * 0.060 + localPosition.x * 0.018 + uTime * 0.55) * 1.45
        + 0.030 * cos((localPosition.x + localPosition.z) * 0.030 - uTime * 0.95) * 0.95
        + 0.22 * cos(localPosition.x * 0.22 + localPosition.z * 0.11 + uTime * 1.7) * 0.28;
    float dz = -0.060 * sin(localPosition.z * 0.060 + localPosition.x * 0.018 + uTime * 0.55) * 1.45
        + 0.030 * cos((localPosition.x + localPosition.z) * 0.030 - uTime * 0.95) * 0.95
        + 0.11 * cos(localPosition.x * 0.22 + localPosition.z * 0.11 + uTime * 1.7) * 0.28;

    vec3 localNormal = normalize(vec3(-dx, 1.0, -dz));
    vec4 worldPosition = uModel * vec4(localPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vWorldNormal = normalize(mat3(transpose(inverse(uModel))) * localNormal);
    vWave = waveA * 0.5 + waveB * 0.35 + waveC * 0.15;

    gl_Position = uProjection * uView * worldPosition;
}
