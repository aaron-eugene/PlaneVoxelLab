///////////////////////////////////////////////////////////////////////////////
// lab_debug/surface_chunk_wireframes.h
// ====================================
//
// Declares debug rendering for surface-containing chunk wireframes.
//
// This module owns the uploaded GPU line mesh used to draw chunk outlines for
// chunks that contain surface-crossing voxels.
//
// This module does not own lab-world data, surface maps, chunks, or renderer
// state.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab_world/lab_world_coordinates.h"
#include "renderer/gpu_mesh.h"

#include <glm/mat4x4.hpp>

#include <vector>

struct Renderer;
struct SurfaceMap;

/***********************************************************
* Surface Chunk Wireframe Types
************************************************************/

struct SurfaceChunkWireframes
{
	GpuMesh chunkWireframeMesh = {};

	std::vector<ChunkCoord> chunkCoords = {};
};

/***********************************************************
* Surface Chunk Wireframe Lifetime
************************************************************/

bool initializeSurfaceChunkWireframes(
	SurfaceChunkWireframes& wireframes,
	const SurfaceMap& surfaceMap);

void shutdownSurfaceChunkWireframes(
	SurfaceChunkWireframes& wireframes);

/***********************************************************
* Surface Chunk Wireframe Rebuild
************************************************************/

bool rebuildSurfaceChunkWireframes(
	SurfaceChunkWireframes& wireframes,
	const SurfaceMap& surfaceMap);

/***********************************************************
* Surface Chunk Wireframe Rendering
************************************************************/

void renderSurfaceChunkWireframes(
	const SurfaceChunkWireframes& wireframes,
	const Renderer& renderer,
	const glm::mat4& viewProjection);
