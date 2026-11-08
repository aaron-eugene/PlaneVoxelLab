///////////////////////////////////////////////////////////////////////////////
// surface_discovery/surface_discovery.h
// =====================================
//
// Declares generic and field-specific surface discovery.
//
// Surface discovery identifies voxels containing density-sign crossings
// and produces chunk-level discovery results for downstream consumers.
//
// This module does not own chunks, density fields, surface maps,
// render resources, or experiment state.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"

#include <cstdint>
#include <vector>

struct Chunk;
struct HeightmapDensityField;

/***********************************************************
* Surface Discovery Types
************************************************************/

struct SurfaceVoxel
{
	VoxelCoord coord = {};
};

struct DiscoveredSurfaceChunk
{
	uint32_t chunkIndex = 0;
	ChunkCoord coord = {};

	std::vector<SurfaceVoxel> voxels = {};
};

/***********************************************************
* Surface Discovery
************************************************************/

void discoverChunkSurfaceVoxels(
	std::vector<SurfaceVoxel>& surfaceVoxels,
	const Chunk& chunk);

void discoverSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks);

// Chunks must contain density samples generated from the supplied heightmap.
// Noncandidate chunks are skipped using their sampled XZ height ranges.
void discoverHeightmapSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks,
	const HeightmapDensityField& heightmap);
