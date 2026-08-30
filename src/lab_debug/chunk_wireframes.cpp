///////////////////////////////////////////////////////////////////////////////
// lab_debug/chunk_wireframes.cpp
// ======================================
//
// Implements debug rendering for surface-containing chunk wireframes.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab_debug/chunk_wireframes.h"

#include "chunk/chunk.h"
#include "renderer/render_vertex.h"
#include "renderer/renderer.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cstdint>
#include <vector>

/***********************************************************
* Constants
************************************************************/

static constexpr uint32_t CHUNK_WIREFRAME_CORNER_COUNT = 8;
static constexpr uint32_t CHUNK_WIREFRAME_EDGE_COUNT = 12;

/***********************************************************
* File-Local Helpers
************************************************************/

static glm::mat4 getChunkModelMatrix(
	const ChunkCoord& chunkCoord)
{
	const glm::dvec3 chunkWorldMin =
		getChunkWorldMin(chunkCoord);

	return glm::translate(
		glm::mat4(1.0f),
		glm::vec3(chunkWorldMin));
}

static void buildChunkWireframeCpuMesh(
	std::vector<ColoredVertex>& vertices,
	std::vector<uint32_t>& indices)
{
	vertices.clear();
	indices.clear();

	vertices.reserve(CHUNK_WIREFRAME_CORNER_COUNT);
	indices.reserve(CHUNK_WIREFRAME_EDGE_COUNT * 2);

	const glm::vec3 color =
		glm::vec3(1.0f, 1.0f, 1.0f);

	const float minCoord = 0.0f;
	const float maxCoord = CHUNK_SIZE_METERS_F;

	const glm::vec3 corners[CHUNK_WIREFRAME_CORNER_COUNT] =
	{
		glm::vec3(minCoord, minCoord, minCoord),
		glm::vec3(maxCoord, minCoord, minCoord),
		glm::vec3(maxCoord, maxCoord, minCoord),
		glm::vec3(minCoord, maxCoord, minCoord),

		glm::vec3(minCoord, minCoord, maxCoord),
		glm::vec3(maxCoord, minCoord, maxCoord),
		glm::vec3(maxCoord, maxCoord, maxCoord),
		glm::vec3(minCoord, maxCoord, maxCoord)
	};

	for (uint32_t cornerIndex = 0;
		cornerIndex < CHUNK_WIREFRAME_CORNER_COUNT;
		++cornerIndex)
	{
		vertices.push_back(
			{ corners[cornerIndex], color });
	}

	const uint32_t edgeIndices[CHUNK_WIREFRAME_EDGE_COUNT][2] =
	{
		{ 0, 1 },
		{ 1, 2 },
		{ 2, 3 },
		{ 3, 0 },

		{ 4, 5 },
		{ 5, 6 },
		{ 6, 7 },
		{ 7, 4 },

		{ 0, 4 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 }
	};

	for (uint32_t edgeIndex = 0;
		edgeIndex < CHUNK_WIREFRAME_EDGE_COUNT;
		++edgeIndex)
	{
		indices.push_back(edgeIndices[edgeIndex][0]);
		indices.push_back(edgeIndices[edgeIndex][1]);
	}
}

static bool createChunkWireframeMesh(
	GpuMesh& mesh)
{
	assert(mesh.vertexArray == 0);
	assert(mesh.vertexBuffer == 0);
	assert(mesh.indexBuffer == 0);

	std::vector<ColoredVertex> vertices = {};
	std::vector<uint32_t> indices = {};

	buildChunkWireframeCpuMesh(
		vertices,
		indices);

	if (!createColoredGpuMesh(
		mesh,
		vertices.data(),
		static_cast<uint32_t>(vertices.size()),
		indices.data(),
		static_cast<uint32_t>(indices.size()),
		GpuPrimitiveType::Lines))
	{
		return false;
	}

	return true;
}

/***********************************************************
* Chunk Wireframe Lifetime
************************************************************/

bool initializeChunkWireframes(
	ChunkWireframes& wireframes,
	const std::vector<Chunk>& chunks)
{
	assert(wireframes.mesh.vertexArray == 0);
	assert(wireframes.mesh.vertexBuffer == 0);
	assert(wireframes.mesh.indexBuffer == 0);
	assert(wireframes.chunkCoords.empty());

	if (!createChunkWireframeMesh(
		wireframes.mesh))
	{
		shutdownChunkWireframes(wireframes);
		return false;
	}

	wireframes.chunkCoords.reserve(
		chunks.size());

	for (const Chunk& chunk : chunks)
	{
		wireframes.chunkCoords.push_back(
			chunk.coord);
	}

	return true;
}

void shutdownChunkWireframes(
	ChunkWireframes& wireframes)
{
	destroyGpuMesh(wireframes.mesh);

	wireframes = {};
}

/***********************************************************
* Chunk Wireframe Rendering
************************************************************/

void renderChunkWireframes(
	const ChunkWireframes& wireframes,
	const Renderer& renderer,
	const glm::mat4& viewProjection)
{
	if (wireframes.mesh.vertexArray == 0 ||
		wireframes.mesh.indexCount == 0)
	{
		return;
	}

	for (const ChunkCoord& chunkCoord : wireframes.chunkCoords)
	{
		const glm::mat4 model =
			getChunkModelMatrix(chunkCoord);

		renderColoredMesh(
			renderer,
			wireframes.mesh,
			model,
			viewProjection);
	}
}
