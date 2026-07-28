///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_planar_cell.cpp
// ===================================================
//
// Implements planar-cell construction and halo-grid access for the XZ
// columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_planar_cell.h"

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "experiments/xz_columnar/xz_columnar_patch.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

/***********************************************************
* Local Planar Cell Constants
************************************************************/

static constexpr int32_t XZ_COLUMNAR_PLANAR_CELL_MIN_COORD = -1;

static constexpr int32_t XZ_COLUMNAR_PLANAR_CELL_MAX_COORD =
	static_cast<int32_t>(CHUNK_SIZE);

static constexpr uint32_t XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE =
	CHUNK_SIZE + (2 * XZ_COLUMNAR_PLANAR_CELL_HALO_SIZE);

static constexpr size_t XZ_COLUMNAR_PLANAR_CELL_COUNT =
	static_cast<size_t>(
		XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE) *
		XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE;

/***********************************************************
* Colorization Helpers
************************************************************/

static glm::vec3 getNormalColor(
	const glm::vec3& normal)
{
	const glm::vec3 absoluteNormal =
		glm::abs(normal);

	return glm::vec3(
		0.55f + 0.25f * absoluteNormal.x,
		0.55f + 0.25f * absoluteNormal.y,
		0.55f + 0.25f * absoluteNormal.z);
}

/***********************************************************
* Planar Cell Grid Helpers
************************************************************/

static uint32_t getPlanarCellGridIndex(
	int32_t relativeX,
	int32_t relativeZ)
{
	assert(relativeX >= XZ_COLUMNAR_PLANAR_CELL_MIN_COORD);
	assert(relativeX <= XZ_COLUMNAR_PLANAR_CELL_MAX_COORD);

	assert(relativeZ >= XZ_COLUMNAR_PLANAR_CELL_MIN_COORD);
	assert(relativeZ <= XZ_COLUMNAR_PLANAR_CELL_MAX_COORD);

	const uint32_t gridX =
		static_cast<uint32_t>(
			relativeX + 1);

	const uint32_t gridZ =
		static_cast<uint32_t>(
			relativeZ + 1);

	return gridX + (gridZ * XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE);
}

/***********************************************************
* Cell Construction Helpers
************************************************************/

static float evaluateTangentPlaneHeight(
	float x,
	float z,
	float centerX,
	float centerZ,
	const XZColumnarPatchSample& sample)
{
	return sample.height +
		sample.gradientX * (x - centerX) +
		sample.gradientZ * (z - centerZ);
}

static XZColumnarPlanarCell buildXZColumnarPlanarCell(
	const HeightmapDensityField& heightmap,
	int32_t relativeX,
	int32_t relativeZ,
	float x0,
	float x1,
	float z0,
	float z1,
	float derivativeStepMeters)
{
	assert(derivativeStepMeters > 0.0f);
	
	const XZColumnarPatchSample patchSample =
		sampleXZColumnarPatchCenter(
			heightmap,
			x0,
			x1,
			z0,
			z1,
			derivativeStepMeters);

	const float centerX =
		(x0 + x1) * 0.5f;

	const float centerZ =
		(z0 + z1) * 0.5f;

	XZColumnarPlanarCell cell = {};
	
	cell.normal =
		glm::normalize(glm::vec3(
			-patchSample.gradientX,
			1.0f,
			-patchSample.gradientZ));

	cell.relativeX = relativeX;
	cell.relativeZ = relativeZ;

	cell.color =
		getNormalColor(
			cell.normal);

	cell.p00 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z0,
				centerX,
				centerZ,
				patchSample),
			z0);

	cell.p01 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z1,
				centerX,
				centerZ,
				patchSample),
			z1);

	cell.p11 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z1,
				centerX,
				centerZ,
				patchSample),
			z1);

	cell.p10 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z0,
				centerX,
				centerZ,
				patchSample),
			z0);

	return cell;
}

/***********************************************************
* XZ Columnar Planar Cell Interface
************************************************************/

void initializeXZColumnarPlanarCellGrid(
	XZColumnarPlanarCellGrid& grid)
{
	assert(grid.cells.empty());

	grid.cells.resize(XZ_COLUMNAR_PLANAR_CELL_COUNT);
}

void buildXZColumnarPlanarCellGrid(
	XZColumnarPlanarCellGrid& grid,
	const HeightmapDensityField& heightmap,
	int32_t chunkX,
	int32_t chunkZ,
	float derivativeStepMeters)
{
	assert(
		grid.cells.size() ==
		XZ_COLUMNAR_PLANAR_CELL_COUNT);

	assert(derivativeStepMeters > 0.0f);

	const float chunkWorldMinX =
		static_cast<float>(chunkX) *
		CHUNK_SIZE_METERS_F;

	const float chunkWorldMinZ =
		static_cast<float>(chunkZ) *
		CHUNK_SIZE_METERS_F;
	
	for (int32_t relativeZ = XZ_COLUMNAR_PLANAR_CELL_MIN_COORD;
		relativeZ <= XZ_COLUMNAR_PLANAR_CELL_MAX_COORD;
		++relativeZ)
	{
		for (int32_t relativeX = XZ_COLUMNAR_PLANAR_CELL_MIN_COORD;
			relativeX <= XZ_COLUMNAR_PLANAR_CELL_MAX_COORD;
			++relativeX)
		{
			const float x0 =
				chunkWorldMinX +
				static_cast<float>(relativeX) *
				VOXEL_SIZE_METERS;

			const float x1 =
				x0 + VOXEL_SIZE_METERS;

			const float z0 =
				chunkWorldMinZ +
				static_cast<float>(relativeZ) *
				VOXEL_SIZE_METERS;

			const float z1 =
				z0 + VOXEL_SIZE_METERS;

			const uint32_t cellIndex =
				getPlanarCellGridIndex(
					relativeX,
					relativeZ);

			grid.cells[cellIndex] =
				buildXZColumnarPlanarCell(
					heightmap,
					relativeX,
					relativeZ,
					x0,
					x1,
					z0,
					z1,
					derivativeStepMeters);
		}
	}
}

const XZColumnarPlanarCell& getXZColumnarPlanarCell(
	const XZColumnarPlanarCellGrid& grid,
	int32_t relativeX,
	int32_t relativeZ)
{
	assert(grid.cells.size() ==
		XZ_COLUMNAR_PLANAR_CELL_COUNT);
	
	const uint32_t index =
		getPlanarCellGridIndex(
			relativeX,
			relativeZ);

	assert(index < grid.cells.size());

	return grid.cells[index];
}

bool isXZColumnarPlanarCellCoordinateOwned(
	int32_t relativeX,
	int32_t relativeZ)
{
	return
		relativeX >= 0 &&
		relativeX <
		static_cast<int32_t>(CHUNK_SIZE) &&
		relativeZ >= 0 &&
		relativeZ <
		static_cast<int32_t>(CHUNK_SIZE);
}

XZColumnarClipPolygon getXZColumnarPlanarCellTopPolygon(
	const XZColumnarPlanarCell& cell)
{
	XZColumnarClipPolygon polygon = {};

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p00, cell.color });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p01, cell.color });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p11, cell.color });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p10, cell.color });

	return polygon;
}
