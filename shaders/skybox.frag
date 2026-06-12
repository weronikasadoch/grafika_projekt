#version 330 core

in vec3 vDirection;

uniform float uTime;

out vec4 fragColor;

float cloudBand(vec2 p, float scale, float speed)
{
    float a = sin(p.x * scale + uTime * speed);
    float b = sin((p.x + p.y) * scale * 0.72 - uTime * speed * 0.61);
    float c = sin((p.x - p.y) * scale * 1.31 + uTime * speed * 0.37);
    return smoothstep(0.42, 0.88, (a + b + c) / 3.0 + 0.5);
}

void main()
{
    vec3 direction = normalize(vDirection);
    float height = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);

    vec3 horizon = vec3(0.62, 0.88, 1.0);
    vec3 zenith = vec3(0.16, 0.55, 0.96);
    vec3 lowerWaterTint = vec3(0.02, 0.30, 0.46);
    vec3 sky = mix(lowerWaterTint, mix(horizon, zenith, height), smoothstep(0.08, 0.45, height));

    vec2 cloudUv = direction.xz / max(direction.y + 0.35, 0.15);
    float clouds = cloudBand(cloudUv, 2.2, 0.08) * smoothstep(0.28, 0.72, height);
    clouds += cloudBand(cloudUv + vec2(2.4, -1.7), 3.8, -0.05) * 0.45 * smoothstep(0.42, 0.88, height);

    vec3 cloudColor = vec3(1.0, 0.98, 0.88);
    sky = mix(sky, cloudColor, clamp(clouds, 0.0, 0.78));

    fragColor = vec4(sky, 1.0);
}
