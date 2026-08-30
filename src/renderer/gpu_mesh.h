///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_mesh.h
// ===================
//
// Declares OpenGL GPU mesh resources and upload helpers for the renderer's
// supported vertex formats.
//
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/render_vertex.h"

#include <cstdint>

/***********************************************************
* GPU Mesh Types
************************************************************/

enum class GpuPrimitiveType
{
	Triangles,
	Lines
};

struct GpuMesh
{
	uint32_t vertexArray = 0;
	uint32_t vertexBuffer = 0;
	uint32_t indexBuffer = 0;

	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;

	GpuPrimitiveType primitiveType = GpuPrimitiveType::Triangles;
};

/***********************************************************
* GPU Mesh Interface
************************************************************/

bool createColoredGpuMesh(
	GpuMesh& mesh,
	const ColoredVertex* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType);

bool createStandardGpuMesh(
	GpuMesh& mesh,
	const StandardVertex* vertices,
	uint32_t vertexCount,
	const uint32_t* indices,
	uint32_t indexCount,
	GpuPrimitiveType primitiveType);

void destroyGpuMesh(GpuMesh& mesh);
