///////////////////////////////////////////////////////////////////////////////
// lab_world.h
// ===========
//
// Defines the shared world data consumed by the surface reference and active
// terrain experiments.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct LabWorld
{

	// Soon:
	// DensityField densityField;
	// Chunk chunks[MAX_LAB_CHUNKS];
	// uint32_t chunkCount;
};

bool initializeLabWorld(LabWorld& world);
void shutdownLabWorld(LabWorld& world);
