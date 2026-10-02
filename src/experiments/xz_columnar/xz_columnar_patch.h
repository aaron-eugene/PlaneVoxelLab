///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_patch.h
// ============================================
//
// Declares bicubic height-patch sampling for the XZ columnar experiment.
//
// A columnar patch approximates a rectangular region of a heightmap using
// bicubic Hermite interpolation. The public interface evaluates the patch at
// its center and returns the height and X/Z gradients used to construct a
// planar tangent approximation.
//
// This module does not construct geometry, determine voxel ownership, perform
// polygon clipping, or own heightmap data.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/field_generators.h"

#include <cstdint>

/***********************************************************
* XZ Columnar Patch Types
************************************************************/

struct XZColumnarPatchSample
{
	float heightOffset = 0.0f;

	float gradientX = 0.0f;
	float gradientZ = 0.0f;
};

/***********************************************************
* XZ Columnar Patch Sampling
************************************************************/

XZColumnarPatchSample sampleXZColumnarPatchCenter(
	const HeightmapDensityField& heightmap,
	int32_t chunkX,
	int32_t chunkZ,
	float localMinX,
	float localMaxX,
	float localMinZ,
	float localMaxZ,
	float derivativeStepMeters);
