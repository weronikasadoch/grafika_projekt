#version 330 core

uniform vec3 uWaterColor;
uniform vec3 uCameraPosition;
uniform float uAlpha;
uniform float uTime;
uniform int uViewedFromBelow;

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in float vWave;

out vec4 fragColor;

void main()
{
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    float fresnel = pow(1.0 - max(dot(normalize(vWorldNormal), viewDir), 0.0), 3.0);
    float ripple = sin(vWorldPosition.x * 0.18 + vWorldPosition.z * 0.08 + uTime * 2.4) * 0.5 + 0.5;
    float foamLine = smoothstep(0.80, 1.0, ripple) * 0.18;

    vec3 shallowColor = vec3(0.10, 0.74, 0.86);
    vec3 deepColor = vec3(0.00, 0.18, 0.42);
    vec3 color = mix(uWaterColor, shallowColor, 0.35 + vWave * 0.18);
    color = mix(color, deepColor, fresnel * 0.35);
    color += vec3(0.65, 0.92, 1.0) * (fresnel * 0.45 + foamLine);

    float alpha = clamp(uAlpha + fresnel * 0.22 + foamLine * 0.35, 0.18, 0.68);
    if (uViewedFromBelow == 1)
    {
        color = mix(color, vec3(0.36, 0.88, 1.0), 0.28);
        color += vec3(0.10, 0.32, 0.38) * foamLine;
        alpha = clamp(uAlpha + fresnel * 0.12 + foamLine * 0.22, 0.16, 0.44);
    }

    fragColor = vec4(color, alpha);
}
