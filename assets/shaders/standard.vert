///////////////////////////////////////////////////////////////////////////////
// standard.vert
// =============
//
// Vertex shader for shaded standard mesh rendering.
//
///////////////////////////////////////////////////////////////////////////////

#version 450 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inTileUv;

uniform mat4 uModel;
uniform mat4 uViewProjection;

out vec3 worldNormal;
out vec3 vertexColor;
out vec2 tileUv;

void main()
{
	worldNormal =
		normalize(
			mat3(uModel) *
			inNormal);

	vertexColor = inColor;
	tileUv = inTileUv;

	gl_Position =
		uViewProjection *
		uModel *
		vec4(inPosition, 1.0);
}
