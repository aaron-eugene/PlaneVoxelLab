#version 450 core

/***********************************************************
* Fragment Inputs
************************************************************/

flat in vec3 flatNormal;
in vec3 smoothNormal;
in vec3 vertexColor;
in vec2 tileUv;

/***********************************************************
* Uniforms
************************************************************/

uniform sampler2D uTexture;

uniform vec3 uLightDirection;
uniform float uAmbientStrength;
uniform float uDiffuseStrength;

uniform float uNormalBlend;

uniform bool uVisualizeNormals;

/***********************************************************
* Fragment Output
************************************************************/

layout(location = 0) out vec4 outColor;

/***********************************************************
* Fragment Shader
************************************************************/

void main()
{
    vec3 normal = normalize(
        mix(
            normalize(flatNormal),
            normalize(smoothNormal),
            clamp(uNormalBlend, 0.0, 1.0)));

    if (uVisualizeNormals)
    {
        outColor = vec4(
            normal * 0.5 + 0.5,
            1.0);

        return;
    }

    vec3 lightDirection =
        normalize(-uLightDirection);

    float diffuse =
        max(
            dot(normal, lightDirection),
            0.0);

    float lighting =
        clamp(
            uAmbientStrength +
            uDiffuseStrength * diffuse,
            0.0,
            1.0);

    vec3 texelColor =
        texture(uTexture, tileUv).rgb;

    vec3 finalColor =
        texelColor *
        vertexColor *
        lighting;

    outColor = vec4(finalColor, 1.0);
}