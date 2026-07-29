///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_mesh.cpp
// =====================
//
// Implements OpenGL GPU mesh creation and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/gpu_mesh.h"

#include "renderer/render_vertex.h"

#include <glad/glad.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

/***********************************************************
* GPU Mesh Creation Constants
************************************************************/

static constexpr uint32_t POSITION_ATTRIBUTE_LOCATION = 0;
static constexpr uint32_t NORMAL_ATTRIBUTE_LOCATION = 1;
static constexpr uint32_t COLOR_ATTRIBUTE_LOCATION = 2;
static constexpr uint32_t TILE_UV_ATTRIBUTE_LOCATION = 3;

static constexpr uint32_t COLORED_VERTEX_COLOR_ATTRIBUTE_LOCATION = 1;

static constexpr uint32_t VERTEX_BUFFER_BINDING_INDEX = 0;

/***********************************************************
* GPU Mesh Creation Helpers
************************************************************/

static void assertEmptyGpuMesh(
	const GpuMesh& mesh)
{
	assert(mesh.vertexArray == 0);
	assert(mesh.vertexBuffer == 0);
	assert(mesh.indexBuffer == 0);
	assert(mesh.vertexCount == 0);
	assert(mesh.indexCount == 0);
}

static void assertValidGpuMeshSource(
	const void* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	uint32_t vertexStride)
{
	assert(vertices != nullptr);
	assert(vertexCount > 0);
	assert(indices != nullptr);
	assert(indexCount > 0);
	assert(vertexStride > 0);
}

static void createGpuMeshStorage(
	GpuMesh& mesh,
	const void* vertices,
	uint32_t vertexCount,
	uint32_t vertexStride,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType)
{
	assertEmptyGpuMesh(mesh);

	assertValidGpuMeshSource(
		vertices,
		vertexCount,
		indices,
		indexCount,
		vertexStride);

	glCreateVertexArrays(
		1,
		&mesh.vertexArray);

	glCreateBuffers(
		1,
		&mesh.vertexBuffer);

	glCreateBuffers(
		1,
		&mesh.indexBuffer);

	// Invalid handles here indicate broken renderer setup or invalid OpenGL state.
	assert(mesh.vertexArray != 0);
	assert(mesh.vertexBuffer != 0);
	assert(mesh.indexBuffer != 0);

	mesh.vertexCount = vertexCount;
	mesh.indexCount = indexCount;
	mesh.primitiveType = primitiveType;

	const size_t vertexDataSize =
		static_cast<size_t>(vertexStride) *
		static_cast<size_t>(vertexCount);

	const size_t indexDataSize =
		sizeof(uint32_t) *
		static_cast<size_t>(indexCount);

	glNamedBufferData(
		mesh.vertexBuffer,
		static_cast<GLsizeiptr>(vertexDataSize),
		vertices,
		GL_STATIC_DRAW);

	glNamedBufferData(
		mesh.indexBuffer,
		static_cast<GLsizeiptr>(indexDataSize),
		indices,
		GL_STATIC_DRAW);

	glVertexArrayVertexBuffer(
		mesh.vertexArray,
		VERTEX_BUFFER_BINDING_INDEX,
		mesh.vertexBuffer,
		0,
		static_cast<GLsizei>(vertexStride));

	glVertexArrayElementBuffer(
		mesh.vertexArray,
		mesh.indexBuffer);
}

static void configureFloatVertexAttribute(
	const GpuMesh& mesh,
	uint32_t attributeLocation,
	int componentCount,
	uint32_t byteOffset)
{
	assert(mesh.vertexArray != 0);
	assert(componentCount > 0);

	glEnableVertexArrayAttrib(
		mesh.vertexArray,
		attributeLocation);

	glVertexArrayAttribFormat(
		mesh.vertexArray,
		attributeLocation,
		componentCount,
		GL_FLOAT,
		GL_FALSE,
		byteOffset);

	glVertexArrayAttribBinding(
		mesh.vertexArray,
		attributeLocation,
		VERTEX_BUFFER_BINDING_INDEX);
}

static void configureColoredVertexLayout(
	const GpuMesh& mesh)
{
	configureFloatVertexAttribute(
		mesh,
		POSITION_ATTRIBUTE_LOCATION,
		3,
		static_cast<uint32_t>(
			offsetof(
				ColoredVertex,
				position)));

	configureFloatVertexAttribute(
		mesh,
		COLORED_VERTEX_COLOR_ATTRIBUTE_LOCATION,
		3,
		static_cast<uint32_t>(
			offsetof(
				ColoredVertex,
				color)));
}

static void configureSurfaceVertexLayout(
	const GpuMesh& mesh)
{
	configureFloatVertexAttribute(
		mesh,
		POSITION_ATTRIBUTE_LOCATION,
		3,
		static_cast<uint32_t>(
			offsetof(
				StandardVertex,
				position)));

	configureFloatVertexAttribute(
		mesh,
		NORMAL_ATTRIBUTE_LOCATION,
		3,
		static_cast<uint32_t>(
			offsetof(
				StandardVertex,
				normal)));

	configureFloatVertexAttribute(
		mesh,
		COLOR_ATTRIBUTE_LOCATION,
		3,
		static_cast<uint32_t>(
			offsetof(
				StandardVertex,
				color)));

	configureFloatVertexAttribute(
		mesh,
		TILE_UV_ATTRIBUTE_LOCATION,
		2,
		static_cast<uint32_t>(
			offsetof(
				StandardVertex,
				tileUv)));
}

/***********************************************************
* GPU Mesh Interface
************************************************************/

bool createColoredGpuMesh(
	GpuMesh& mesh,
	const ColoredVertex* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType)
{
	createGpuMeshStorage(
		mesh,
		vertices,
		vertexCount,
		static_cast<uint32_t>(
			sizeof(ColoredVertex)),
		indices,
		indexCount,
		primitiveType);

	configureColoredVertexLayout(mesh);

	return true;
}

bool createStandardGpuMesh(
	GpuMesh& mesh,
	const StandardVertex* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType)
{
	createGpuMeshStorage(
		mesh,
		vertices,
		vertexCount,
		static_cast<uint32_t>(
			sizeof(StandardVertex)),
		indices,
		indexCount,
		primitiveType);

	configureSurfaceVertexLayout(mesh);

	return true;
}

void destroyGpuMesh(
	GpuMesh& mesh)
{
	if (mesh.indexBuffer != 0)
	{
		glDeleteBuffers(
			1,
			&mesh.indexBuffer);
	}

	if (mesh.vertexBuffer != 0)
	{
		glDeleteBuffers(
			1,
			&mesh.vertexBuffer);
	}

	if (mesh.vertexArray != 0)
	{
		glDeleteVertexArrays(
			1,
			&mesh.vertexArray);
	}

	mesh = {};
}
