///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_mesh.cpp
// =====================
//
// Implements OpenGL GPU mesh creation and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/gpu_mesh.h"

#include <glad/glad.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

/***********************************************************
* GPU Mesh Interface
************************************************************/

bool createGpuMesh(
	GpuMesh& mesh,
	const ColoredVertex* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType)
{
	assert(mesh.vertexArray == 0);
	assert(mesh.vertexBuffer == 0);
	assert(mesh.indexBuffer == 0);
	assert(mesh.vertexCount == 0);
	assert(mesh.indexCount == 0);

	assert(vertices != nullptr);
	assert(vertexCount > 0);
	assert(indices != nullptr);
	assert(indexCount > 0);

	glCreateVertexArrays(1, &mesh.vertexArray);
	glCreateBuffers(1, &mesh.vertexBuffer);
	glCreateBuffers(1, &mesh.indexBuffer);

	// Invalid handles here indicate broken renderer setup or invalid OpenGL state.
	assert(mesh.vertexArray != 0);
	assert(mesh.vertexBuffer != 0);
	assert(mesh.indexBuffer != 0);

	mesh.vertexCount = vertexCount;
	mesh.indexCount = indexCount;
	mesh.primitiveType = primitiveType;

	glNamedBufferData(
		mesh.vertexBuffer,
		sizeof(ColoredVertex) * vertexCount,
		vertices,
		GL_STATIC_DRAW);

	glNamedBufferData(
		mesh.indexBuffer,
		sizeof(uint32_t) * indexCount,
		indices,
		GL_STATIC_DRAW);

	glVertexArrayVertexBuffer(
		mesh.vertexArray,
		0,
		mesh.vertexBuffer,
		0,
		sizeof(ColoredVertex));

	glEnableVertexArrayAttrib(mesh.vertexArray, 0);
	glVertexArrayAttribFormat(
		mesh.vertexArray,
		0,
		3,
		GL_FLOAT,
		GL_FALSE,
		static_cast<uint32_t>(offsetof(ColoredVertex, position)));
	glVertexArrayAttribBinding(mesh.vertexArray, 0, 0);

	glEnableVertexArrayAttrib(mesh.vertexArray, 1);
	glVertexArrayAttribFormat(
		mesh.vertexArray,
		1,
		3,
		GL_FLOAT,
		GL_FALSE,
		static_cast<uint32_t>(offsetof(ColoredVertex, color)));
	glVertexArrayAttribBinding(mesh.vertexArray, 1, 0);

	glVertexArrayElementBuffer(mesh.vertexArray, mesh.indexBuffer);

	return true;
}

void destroyGpuMesh(GpuMesh& mesh)
{
	if (mesh.indexBuffer != 0)
	{
		glDeleteBuffers(1, &mesh.indexBuffer);
	}

	if (mesh.vertexBuffer != 0)
	{
		glDeleteBuffers(1, &mesh.vertexBuffer);
	}

	if (mesh.vertexArray != 0)
	{
		glDeleteVertexArrays(1, &mesh.vertexArray);
	}

	mesh = {};
}
