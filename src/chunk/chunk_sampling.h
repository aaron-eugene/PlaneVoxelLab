///////////////////////////////////////////////////////////////////////////////
// chunk/chunk_sampling.h
// ======================
//
// Declares helpers for sampling density fields into chunks.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <array>

struct Chunk;
struct DensityField;
struct HeightmapDensityField;

/***********************************************************
* General Density Sampling
************************************************************/

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field);

/***********************************************************
* Heightmap Sampling Types
************************************************************/

// Temporary XZ height samples for one horizontal chunk footprint.
// Includes the shared boundary samples on the positive X and Z edges.
// Indexing is X-major; the grid can be reused across vertical chunks.
struct ChunkHeightRange
{
	WorldMetricCoordinate minHeight = {};
	WorldMetricCoordinate maxHeight = {};
};

struct ChunkHeightGrid
{
	std::array<
		WorldMetricCoordinate,
		CHUNK_SAMPLE_SIZE * CHUNK_SAMPLE_SIZE> heights = {};
};

/***********************************************************
* Heightmap Sampling
************************************************************/

// Samples terrain heights over the XZ footprint of chunkCoord.
// Requires sampleHeightmapTerrainHeight() to be independent of Y.
// Callers conventionally pass chunkCoord.y = 0.
void sampleChunkHeightGrid(
	ChunkHeightGrid& heightGrid,
	const ChunkCoord& chunkCoord,
	const HeightmapDensityField& heightmap);

void populateChunkDensityFromHeightGrid(
	Chunk& chunk,
	const ChunkHeightGrid& heightGrid);

ChunkHeightRange getChunkHeightRange(
	const ChunkHeightGrid& heightGrid);

