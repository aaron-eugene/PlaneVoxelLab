///////////////////////////////////////////////////////////////////////////////
// lab_debug/chunk_wireframes.h
// ====================================
//
// Declares debug rendering for loaded chunk wireframes.
//
// This module owns the uploaded GPU line mesh used to draw chunk outlines for
// all currently loaded chunks.
//
// This module does not own lab-world data, surface maps, chunks, or renderer
// state.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"
#include "renderer/gpu_mesh.h"

#include <glm/mat4x4.hpp>

#include <vector>

struct Chunk;
struct Renderer;

/***********************************************************
* Chunk Wireframe Types
************************************************************/

struct ChunkWireframes
{
	GpuMesh mesh = {};

	std::vector<ChunkCoord> chunkCoords = {};
};

/***********************************************************
* Chunk Wireframe Lifetime
************************************************************/

bool initializeChunkWireframes(
	ChunkWireframes& wireframes,
	const std::vector<Chunk>& chunks);

void shutdownChunkWireframes(
	ChunkWireframes& wireframes);

/***********************************************************
* Chunk Wireframe Rendering
************************************************************/

void renderChunkWireframes(
	const ChunkWireframes& wireframes,
	const Renderer& renderer,
	const glm::mat4& viewProjection);
