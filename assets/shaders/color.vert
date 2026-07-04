///////////////////////////////////////////////////////////////////////////////
// color.vert
// ==========
//
// Vertex shader for colored mesh rendering.
//
///////////////////////////////////////////////////////////////////////////////

#version 450 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

uniform mat4 uModel;
uniform mat4 uViewProjection;

out vec3 vertexColor;

void main()
{
	vertexColor = inColor;

	gl_Position =
		uViewProjection *
		uModel *
		vec4(inPosition, 1.0);
}
