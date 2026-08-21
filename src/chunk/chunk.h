///////////////////////////////////////////////////////////////////////////////
// chunk/chunk.h
// =============
//
// Declares chunk data used by lab-world terrain sampling and surface extraction.
//
// A chunk owns density samples at voxel-grid corner positions. For a chunk with
// N voxels per axis, the sampled corner grid has N + 1 samples per axis.
//
// Chunks do not own render meshes, surface extraction results, or active
// experiment data. Those systems derive their data from the chunk's sampled
// density field.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"

#include <vector>

/***********************************************************
* Chunk Types
************************************************************/

struct Chunk
{
	ChunkCoord coord = {};

	std::vector<float> densitySamples = {};
};

/***********************************************************
* Chunk Initialization
************************************************************/

void initializeChunk(
	Chunk& chunk,
	const ChunkCoord& coord);

/***********************************************************
* Chunk Density Sample Access
************************************************************/

float getChunkDensitySample(
	const Chunk& chunk,
	const SampleCoord& sampleCoord);

void setChunkDensitySample(
	Chunk& chunk,
	const SampleCoord& sampleCoord,
	float density);
