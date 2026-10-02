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
#include "terrain_render/terrain_tile_atlas.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

/***********************************************************
* Surface Tile Classification Constants
************************************************************/

static constexpr WorldMetricCoordinate
SAND_MAX_WORLD_HEIGHT =
{
	-3,
	0.0f
};

static constexpr WorldMetricCoordinate
GRASS_MAX_WORLD_HEIGHT =
{
	6,
	0.0f
};

static constexpr WorldMetricCoordinate
ROCK_MAX_WORLD_HEIGHT =
{
	12,
	0.0f
};

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
* Surface Tile Helpers
************************************************************/

static TerrainTile classifyXZColumnarSurfaceTile(
	const WorldMetricCoordinate& worldHeight)
{
	if (compareWorldMetricCoordinates(
		worldHeight,
		SAND_MAX_WORLD_HEIGHT) < 0)
	{
		return TerrainTile::Sand;
	}

	if (compareWorldMetricCoordinates(
		worldHeight,
		GRASS_MAX_WORLD_HEIGHT) < 0)
	{
		return TerrainTile::Grass;
	}

	if (compareWorldMetricCoordinates(
		worldHeight,
		ROCK_MAX_WORLD_HEIGHT) < 0)
	{
		return TerrainTile::Rock;
	}

	return TerrainTile::Snow;
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

static float evaluateTangentPlaneHeightOffset(
	float x,
	float z,
	float centerX,
	float centerZ,
	const XZColumnarPatchSample& sample)
{
	return sample.heightOffset +
		sample.gradientX * (x - centerX) +
		sample.gradientZ * (z - centerZ);
}

static XZColumnarPlanarCell buildXZColumnarPlanarCell(
	const HeightmapDensityField& heightmap,
	int32_t chunkX,
	int32_t chunkZ,
	int32_t relativeX,
	int32_t relativeZ,
	float localX0,
	float localX1,
	float localZ0,
	float localZ1,
	float derivativeStepMeters)
{
	assert(derivativeStepMeters > 0.0f);
	
	const XZColumnarPatchSample patchSample =
		sampleXZColumnarPatchCenter(
			heightmap,
			chunkX,
			chunkZ,
			localX0,
			localX1,
			localZ0,
			localZ1,
			derivativeStepMeters);

	const float localCenterX =
		(localX0 + localX1) * 0.5f;

	const float localCenterZ =
		(localZ0 + localZ1) * 0.5f;

	XZColumnarPlanarCell cell = {};
	
	cell.normal =
		glm::normalize(glm::vec3(
			-patchSample.gradientX,
			1.0f,
			-patchSample.gradientZ));

	cell.relativeX = relativeX;
	cell.relativeZ = relativeZ;

	cell.surfaceCenterHeightOffset =
		patchSample.heightOffset;

	// Tile classification
	const WorldMetricCoordinate surfaceCenterHeight =
		offsetWorldMetricCoordinate(
			heightmap.baseHeight,
			patchSample.heightOffset);

	cell.surfaceTile =
		classifyXZColumnarSurfaceTile(
			surfaceCenterHeight);

	cell.p00 =
		glm::vec3(
			localX0,
			evaluateTangentPlaneHeightOffset(
				localX0,
				localZ0,
				localCenterX,
				localCenterZ,
				patchSample),
			localZ0);

	cell.p01 =
		glm::vec3(
			localX0,
			evaluateTangentPlaneHeightOffset(
				localX0,
				localZ1,
				localCenterX,
				localCenterZ,
				patchSample),
			localZ1);

	cell.p11 =
		glm::vec3(
			localX1,
			evaluateTangentPlaneHeightOffset(
				localX1,
				localZ1,
				localCenterX,
				localCenterZ,
				patchSample),
			localZ1);

	cell.p10 =
		glm::vec3(
			localX1,
			evaluateTangentPlaneHeightOffset(
				localX1,
				localZ0,
				localCenterX,
				localCenterZ,
				patchSample),
			localZ0);

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

	grid.baseHeight = heightmap.baseHeight;

	for (int32_t relativeZ =
		XZ_COLUMNAR_PLANAR_CELL_MIN_COORD;
		relativeZ <=
		XZ_COLUMNAR_PLANAR_CELL_MAX_COORD;
		++relativeZ)
	{
		for (int32_t relativeX =
			XZ_COLUMNAR_PLANAR_CELL_MIN_COORD;
			relativeX <=
			XZ_COLUMNAR_PLANAR_CELL_MAX_COORD;
			++relativeX)
		{
			const float localX0 =
				static_cast<float>(relativeX) *
				VOXEL_SIZE_METERS;

			const float localX1 =
				localX0 +
				VOXEL_SIZE_METERS;

			const float localZ0 =
				static_cast<float>(relativeZ) *
				VOXEL_SIZE_METERS;

			const float localZ1 =
				localZ0 +
				VOXEL_SIZE_METERS;

			const uint32_t cellIndex =
				getPlanarCellGridIndex(
					relativeX,
					relativeZ);

			grid.cells[cellIndex] =
				buildXZColumnarPlanarCell(
					heightmap,
					chunkX,
					chunkZ,
					relativeX,
					relativeZ,
					localX0,
					localX1,
					localZ0,
					localZ1,
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
		{ cell.p00 });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p01 });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p11 });

	appendXZColumnarClipVertex(
		polygon,
		{ cell.p10 });

	return polygon;
}
