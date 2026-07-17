///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_patch.cpp
// ==============================================
//
// Implements bicubic height-patch sampling for the XZ columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_patch.h"

#include "fields/field_generators.h"

#include <cassert>
#include <cstdint>

/***********************************************************
* File-Local Types
************************************************************/

namespace
{
	struct XZColumnarPatchCorner
	{
		float x = 0.0f;
		float z = 0.0f;

		float height = 0.0f;

		float gradientX = 0.0f;
		float gradientZ = 0.0f;
		float gradientXZ = 0.0f;
	};
}

/***********************************************************
* Height Sampling Helpers
************************************************************/

static float sampleHeight(
	const HeightmapDensityField& heightmap,
	float x,
	float z)
{
	return sampleHeightmapTerrainHeight(
		heightmap,
		x,
		z);
}

static XZColumnarPatchCorner samplePatchCorner(
	const HeightmapDensityField& heightmap,
	float x,
	float z,
	float derivativeStepMeters)
{
	assert(derivativeStepMeters > 0.0f);

	XZColumnarPatchCorner corner = {};
	corner.x = x;
	corner.z = z;

	corner.height =
		sampleHeight(
			heightmap,
			x,
			z);

	const float step =
		derivativeStepMeters;

	const float heightX0 =
		sampleHeight(
			heightmap,
			x - step,
			z);

	const float heightX1 =
		sampleHeight(
			heightmap,
			x + step,
			z);

	const float heightZ0 =
		sampleHeight(
			heightmap,
			x,
			z - step);

	const float heightZ1 =
		sampleHeight(
			heightmap,
			x,
			z + step);

	corner.gradientX =
		(heightX1 - heightX0) /
		(2.0f * step);

	corner.gradientZ =
		(heightZ1 - heightZ0) /
		(2.0f * step);

	const float heightX0Z0 =
		sampleHeight(
			heightmap,
			x - step,
			z - step);

	const float heightX0Z1 =
		sampleHeight(
			heightmap,
			x - step,
			z + step);

	const float heightX1Z0 =
		sampleHeight(
			heightmap,
			x + step,
			z - step);

	const float heightX1Z1 =
		sampleHeight(
			heightmap,
			x + step,
			z + step);

	corner.gradientXZ =
		(heightX1Z1 -
			heightX1Z0 -
			heightX0Z1 +
			heightX0Z0) /
		(4.0f * step * step);

	return corner;
}

/***********************************************************
* Hermite Basis Helpers
************************************************************/

static void evaluateHermiteBasis(
	float t,
	float basis[4],
	float derivativeBasis[4])
{
	const float t2 = t * t;
	const float t3 = t2 * t;

	basis[0] =
		2.0f * t3 -
		3.0f * t2 +
		1.0f;

	basis[1] =
		-2.0f * t3 +
		3.0f * t2;

	basis[2] =
		t3 -
		2.0f * t2 +
		t;

	basis[3] =
		t3 -
		t2;

	derivativeBasis[0] =
		6.0f * t2 -
		6.0f * t;

	derivativeBasis[1] =
		-6.0f * t2 +
		6.0f * t;

	derivativeBasis[2] =
		3.0f * t2 -
		4.0f * t +
		1.0f;

	derivativeBasis[3] =
		3.0f * t2 -
		2.0f * t;
}

/***********************************************************
* Bicubic Patch Helpers
************************************************************/

static XZColumnarPatchSample evaluateBicubicPatchCenter(
	const XZColumnarPatchCorner& corner00,
	const XZColumnarPatchCorner& corner10,
	const XZColumnarPatchCorner& corner01,
	const XZColumnarPatchCorner& corner11)
{
	const float patchSizeX =
		corner10.x - corner00.x;

	const float patchSizeZ =
		corner01.z - corner00.z;

	assert(patchSizeX > 0.0f);
	assert(patchSizeZ > 0.0f);

	float basisU[4] = {};
	float derivativeBasisU[4] = {};

	float basisV[4] = {};
	float derivativeBasisV[4] = {};

	evaluateHermiteBasis(
		0.5f,
		basisU,
		derivativeBasisU);

	evaluateHermiteBasis(
		0.5f,
		basisV,
		derivativeBasisV);

	// Bicubic Hermite data matrix:
	// [ h00, h01, hz00, hz01 ]
	// [ h10, h11, hz10, hz11 ]
	// [ hx00, hx01, hxz00, hxz01 ]
	// [ hx10, hx11, hxz10, hxz11 ]
	float patchData[4][4] = {};

	patchData[0][0] =
		corner00.height;

	patchData[1][0] =
		corner10.height;

	patchData[0][1] =
		corner01.height;

	patchData[1][1] =
		corner11.height;

	patchData[2][0] =
		corner00.gradientX *
		patchSizeX;

	patchData[3][0] =
		corner10.gradientX *
		patchSizeX;

	patchData[2][1] =
		corner01.gradientX *
		patchSizeX;

	patchData[3][1] =
		corner11.gradientX *
		patchSizeX;

	patchData[0][2] =
		corner00.gradientZ *
		patchSizeZ;

	patchData[1][2] =
		corner10.gradientZ *
		patchSizeZ;

	patchData[0][3] =
		corner01.gradientZ *
		patchSizeZ;

	patchData[1][3] =
		corner11.gradientZ *
		patchSizeZ;

	patchData[2][2] =
		corner00.gradientXZ *
		patchSizeX *
		patchSizeZ;

	patchData[3][2] =
		corner10.gradientXZ *
		patchSizeX *
		patchSizeZ;

	patchData[2][3] =
		corner01.gradientXZ *
		patchSizeX *
		patchSizeZ;

	patchData[3][3] =
		corner11.gradientXZ *
		patchSizeX *
		patchSizeZ;

	XZColumnarPatchSample sample = {};

	float derivativeU = 0.0f;
	float derivativeV = 0.0f;

	for (uint32_t uIndex = 0;
		uIndex < 4;
		++uIndex)
	{
		for (uint32_t vIndex = 0;
			vIndex < 4;
			++vIndex)
		{
			const float value =
				patchData[uIndex][vIndex];

			sample.height +=
				basisU[uIndex] *
				basisV[vIndex] *
				value;

			derivativeU +=
				derivativeBasisU[uIndex] *
				basisV[vIndex] *
				value;

			derivativeV +=
				basisU[uIndex] *
				derivativeBasisV[vIndex] *
				value;
		}
	}

	sample.gradientX =
		derivativeU /
		patchSizeX;

	sample.gradientZ =
		derivativeV /
		patchSizeZ;

	return sample;
}

/***********************************************************
* XZ Columnar Patch Sampling
************************************************************/

XZColumnarPatchSample sampleXZColumnarPatchCenter(
	const HeightmapDensityField& heightmap,
	float minX,
	float maxX,
	float minZ,
	float maxZ,
	float derivativeStepMeters)
{
	assert(maxX > minX);
	assert(maxZ > minZ);
	assert(derivativeStepMeters > 0.0f);

	const XZColumnarPatchCorner corner00 =
		samplePatchCorner(
			heightmap,
			minX,
			minZ,
			derivativeStepMeters);

	const XZColumnarPatchCorner corner10 =
		samplePatchCorner(
			heightmap,
			maxX,
			minZ,
			derivativeStepMeters);

	const XZColumnarPatchCorner corner01 =
		samplePatchCorner(
			heightmap,
			minX,
			maxZ,
			derivativeStepMeters);

	const XZColumnarPatchCorner corner11 =
		samplePatchCorner(
			heightmap,
			maxX,
			maxZ,
			derivativeStepMeters);

	return evaluateBicubicPatchCenter(
		corner00,
		corner10,
		corner01,
		corner11);
}
