#version 450 core

/***********************************************************
* Vertex Attributes
************************************************************/

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inFlatNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTileUv;
layout(location = 4) in vec3 inSmoothNormal;

/***********************************************************
* Uniforms
************************************************************/

uniform mat4 uModel;
uniform mat4 uViewProjection;

/***********************************************************
* Vertex Outputs
************************************************************/

flat out vec3 flatNormal;
out vec3 smoothNormal;
out vec3 vertexColor;
out vec2 tileUv;

/***********************************************************
* Vertex Shader
************************************************************/

void main()
{
    mat3 normalMatrix = mat3(uModel);

    flatNormal = normalize(
        normalMatrix * inFlatNormal);

    smoothNormal = normalize(
        normalMatrix * inSmoothNormal);

    vertexColor = inColor;
    tileUv = inTileUv;

    gl_Position =
        uViewProjection *
        uModel *
        vec4(inPosition, 1.0);
}