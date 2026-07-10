///////////////////////////////////////////////////////////////////////////////
// chunk/chunk.cpp
// ===============
//
///////////////////////////////////////////////////////////////////////////////

#include "chunk/chunk.h"

#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"

#include <cassert>
#include <cstdint>

/***********************************************************
* Chunk Lifetime
************************************************************/

bool initializeChunk(
	Chunk& chunk,
	const ChunkCoord& coord)
{
	assert(chunk.densitySamples.empty());

	chunk = {};
	chunk.coord = coord;

	chunk.densitySamples.resize(CHUNK_SAMPLE_COUNT);

	return chunk.densitySamples.size() == CHUNK_SAMPLE_COUNT;
}

/***********************************************************
* Chunk Density Sample Access
************************************************************/

float getChunkDensitySample(
	const Chunk& chunk,
	const SampleCoord& sampleCoord)
{
	assert(chunk.densitySamples.size() == CHUNK_SAMPLE_COUNT);
	assert(isSampleCoordInChunkBounds(sampleCoord));

	const uint32_t sampleIndex =
		getSampleIndexInChunk(sampleCoord);

	return chunk.densitySamples[sampleIndex];
}

void setChunkDensitySample(
	Chunk& chunk,
	const SampleCoord& sampleCoord,
	float density)
{
	assert(chunk.densitySamples.size() == CHUNK_SAMPLE_COUNT);
	assert(isSampleCoordInChunkBounds(sampleCoord));

	const uint32_t sampleIndex =
		getSampleIndexInChunk(sampleCoord);

	chunk.densitySamples[sampleIndex] = density;
}