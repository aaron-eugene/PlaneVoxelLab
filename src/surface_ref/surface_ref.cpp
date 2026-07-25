///////////////////////////////////////////////////////////////////////////////
// surface_ref/surface_ref.cpp
// ===========================
//
// Implements reference surface mesh creation, destruction, rebuilding, and
// rendering.
//
///////////////////////////////////////////////////////////////////////////////

#include "surface_ref/surface_ref.h"

#include "chunk/chunk.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/renderer.h"
#include "surface/surface_map.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cstdint>
#include <utility>

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

static void destroySurfaceRefChunk(
	SurfaceRefChunk& surfaceRefChunk)
{
	destroyGpuMesh(surfaceRefChunk.gpuMesh);
	clearTetrahedraMesh(surfaceRefChunk.cpuMesh);

	surfaceRefChunk = {};
}

static bool rebuildSurfaceRefChunk(
	SurfaceRefChunk& surfaceRefChunk,
	const Chunk& chunk,
	const SurfaceChunk& surfaceChunk)
{
	assert(surfaceChunk.coord.x == chunk.coord.x);
	assert(surfaceChunk.coord.y == chunk.coord.y);
	assert(surfaceChunk.coord.z == chunk.coord.z);

	destroyGpuMesh(surfaceRefChunk.gpuMesh);
	clearTetrahedraMesh(surfaceRefChunk.cpuMesh);

	surfaceRefChunk.coord = chunk.coord;

	if (!buildTetrahedraMesh(
		surfaceRefChunk.cpuMesh,
		chunk,
		surfaceChunk))
	{
		return false;
	}

	if (surfaceRefChunk.cpuMesh.vertices.empty() ||
		surfaceRefChunk.cpuMesh.indices.empty())
	{
		return true;
	}

	const uint32_t vertexCount =
		static_cast<uint32_t>(surfaceRefChunk.cpuMesh.vertices.size());

	const uint32_t indexCount =
		static_cast<uint32_t>(surfaceRefChunk.cpuMesh.indices.size());

	if (!createColoredGpuMesh(
		surfaceRefChunk.gpuMesh,
		surfaceRefChunk.cpuMesh.vertices.data(),
		vertexCount,
		surfaceRefChunk.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles))
	{
		destroyGpuMesh(surfaceRefChunk.gpuMesh);
		return false;
	}

	return true;
}

/***********************************************************
* Surface Reference Lifetime
************************************************************/

bool initializeSurfaceRef(
	SurfaceRef& surfaceRef,
	const std::vector<Chunk>& chunks,
	const SurfaceMap& surfaceMap)
{
	assert(surfaceRef.chunks.empty());

	return rebuildSurfaceRef(
		surfaceRef,
		chunks,
		surfaceMap);
}

void shutdownSurfaceRef(
	SurfaceRef& surfaceRef)
{
	for (SurfaceRefChunk& surfaceRefChunk : surfaceRef.chunks)
	{
		destroySurfaceRefChunk(surfaceRefChunk);
	}

	surfaceRef = {};
}

/***********************************************************
* Surface Reference Mesh Rebuild
************************************************************/

bool rebuildSurfaceRef(
	SurfaceRef& surfaceRef,
	const std::vector<Chunk>& chunks,
	const SurfaceMap& surfaceMap)
{
	shutdownSurfaceRef(surfaceRef);

	surfaceRef.chunks.reserve(surfaceMap.chunks.size());

	for (const SurfaceChunk& surfaceChunk : surfaceMap.chunks)
	{
		assert(surfaceChunk.chunkIndex < chunks.size());

		const Chunk& chunk =
			chunks[surfaceChunk.chunkIndex];

		assert(surfaceChunk.coord.x == chunk.coord.x);
		assert(surfaceChunk.coord.y == chunk.coord.y);
		assert(surfaceChunk.coord.z == chunk.coord.z);

		SurfaceRefChunk surfaceRefChunk = {};
		surfaceRefChunk.coord = surfaceChunk.coord;

		if (!rebuildSurfaceRefChunk(
			surfaceRefChunk,
			chunk,
			surfaceChunk))
		{
			destroySurfaceRefChunk(surfaceRefChunk);
			shutdownSurfaceRef(surfaceRef);
			return false;
		}

		// Keep a SurfaceRefChunk even if the sparse chunk unexpectedly produces
		// no triangles. That preserves one-to-one correspondence with the
		// surface map for debugging.
		surfaceRef.chunks.push_back(std::move(surfaceRefChunk));
	}

	return true;
}

/***********************************************************
* Surface Reference Rendering
************************************************************/

void renderSurfaceRef(
	const SurfaceRef& surfaceRef,
	const Renderer& renderer,
	const glm::mat4& viewProjection)
{
	for (const SurfaceRefChunk& surfaceRefChunk : surfaceRef.chunks)
	{
		if (surfaceRefChunk.gpuMesh.vertexArray == 0 ||
			surfaceRefChunk.gpuMesh.indexCount == 0)
		{
			continue;
		}

		const glm::mat4 model =
			getChunkModelMatrix(surfaceRefChunk.coord);

		renderColoredMesh(
			surfaceRefChunk.gpuMesh,
			renderer.colorShader,
			model,
			viewProjection);
	}
}
