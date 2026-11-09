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
struct ChunkHeightGrid;
struct HeightmapDensityField;

/***********************************************************
* Surface Discovery Types
************************************************************/

// Identifies a surface-crossing voxel by its chunk-local coordinate.
// Discovered surface voxels have no guaranteed traversal order.
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
* Voxel Surface Discovery
************************************************************/

void discoverChunkSurfaceVoxels(
	std::vector<SurfaceVoxel>& surfaceVoxels,
	const Chunk& chunk);

void discoverHeightmapChunkSurfaceVoxels(
	std::vector<SurfaceVoxel>& surfaceVoxels,
	const Chunk& chunk,
	const ChunkHeightGrid& heightGrid);

/***********************************************************
* Chunk Surface Discovery
************************************************************/

void discoverSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks);

// Chunks must contain density samples generated from the supplied heightmap.
// Noncandidate chunks are skipped using their sampled XZ height ranges.
void discoverHeightmapSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks,
	const HeightmapDensityField& heightmap);
