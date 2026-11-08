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
* Heightmap Sampling Types
************************************************************/

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

void sampleChunkHeightGrid(
	ChunkHeightGrid& heightGrid,
	const ChunkCoord& chunkCoord,
	const HeightmapDensityField& heightmap);

void populateChunkDensityFromHeightGrid(
	Chunk& chunk,
	const ChunkHeightGrid& heightGrid);

ChunkHeightRange getChunkHeightRange(
	const ChunkHeightGrid& heightGrid);

/***********************************************************
* General Density Sampling
************************************************************/

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field);
