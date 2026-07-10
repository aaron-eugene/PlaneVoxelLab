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

/***********************************************************
* Chunk Sampling Interface
************************************************************/

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field);
