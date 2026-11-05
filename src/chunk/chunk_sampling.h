///////////////////////////////////////////////////////////////////////////////
// chunk/chunk_sampling.h
// ======================
//
// Declares helpers for sampling density fields into chunks.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct Chunk;
struct DensityField;
struct HeightmapDensityField;

/***********************************************************
* Chunk Sampling Interface
************************************************************/

// Optimized for single heightmap
void sampleChunkHeightmapDensityField(
	Chunk& chunk,
	const HeightmapDensityField& heightmap);

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field);
