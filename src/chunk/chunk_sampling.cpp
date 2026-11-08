///////////////////////////////////////////////////////////////////////////////
// chunk/chunk_sampling.cpp
// ========================
//
// Implements density-field sampling for chunk sample grids.
//
///////////////////////////////////////////////////////////////////////////////

#include "chunk/chunk_sampling.h"

#include "chunk/chunk.h"
#include "fields/density_field.h"
#include "fields/field_generators.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <cstdint>

/***********************************************************
* General Density Sampling
************************************************************/

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field)
{
	for (uint32_t sampleZ = 0;
		sampleZ < CHUNK_SAMPLE_SIZE;
		++sampleZ)
	{
		for (uint32_t sampleY = 0;
			sampleY < CHUNK_SAMPLE_SIZE;
			++sampleY)
		{
			for (uint32_t sampleX = 0;
				sampleX < CHUNK_SAMPLE_SIZE;
				++sampleX)
			{
				const SampleCoord sampleCoord =
				{
					sampleX,
					sampleY,
					sampleZ
				};

				const WorldPosition sampleWorldPosition =
					getSampleWorldPosition(
						chunk.coord,
						sampleCoord);

				const float density =
					sampleDensityField(
						field,
						sampleWorldPosition);

				setChunkDensitySample(
					chunk,
					sampleCoord,
					density);
			}
		}
	}
}

/***********************************************************
* Heightmap Sampling
************************************************************/

void sampleChunkHeightGrid(
	ChunkHeightGrid& heightGrid,
	const ChunkCoord& chunkCoord,
	const HeightmapDensityField& heightmap)
{
	for (uint32_t sampleZ = 0;
		sampleZ < CHUNK_SAMPLE_SIZE;
		++sampleZ)
	{
		for (uint32_t sampleX = 0;
			sampleX < CHUNK_SAMPLE_SIZE;
			++sampleX)
		{
			const SampleCoord sampleCoord =
			{
				sampleX,
				0,
				sampleZ
			};

			const WorldPosition worldPosition =
				getSampleWorldPosition(
					chunkCoord,
					sampleCoord);

			const WorldMetricCoordinate terrainHeight =
				sampleHeightmapTerrainHeight(
					heightmap,
					worldPosition);

			const uint32_t heightIndex =
				sampleX +
				sampleZ * CHUNK_SAMPLE_SIZE;

			heightGrid.heights[heightIndex] =
				terrainHeight;
		}
	}
}

void populateChunkDensityFromHeightGrid(
	Chunk& chunk,
	const ChunkHeightGrid& heightGrid)
{
	for (uint32_t sampleZ = 0;
		sampleZ < CHUNK_SAMPLE_SIZE;
		++sampleZ)
	{
		for (uint32_t sampleY = 0;
			sampleY < CHUNK_SAMPLE_SIZE;
			++sampleY)
		{
			const SampleCoord ySampleCoord =
			{
				0,
				sampleY,
				0
			};

			const WorldPosition yWorldPosition =
				getSampleWorldPosition(
					chunk.coord,
					ySampleCoord);

			const WorldMetricCoordinate worldY =
				getWorldMetricCoordinate(
					yWorldPosition.chunk.y,
					yWorldPosition.localPosition.y);

			for (uint32_t sampleX = 0;
				sampleX < CHUNK_SAMPLE_SIZE;
				++sampleX)
			{
				const uint32_t heightIndex =
					sampleX +
					sampleZ * CHUNK_SAMPLE_SIZE;

				const WorldMetricCoordinate& terrainHeight =
					heightGrid.heights[heightIndex];

				const float density =
					getWorldMetricCoordinateOffset(
						terrainHeight,
						worldY);

				const SampleCoord sampleCoord =
				{
					sampleX,
					sampleY,
					sampleZ
				};

				setChunkDensitySample(
					chunk,
					sampleCoord,
					density);
			}
		}
	}
}

ChunkHeightRange getChunkHeightRange(
	const ChunkHeightGrid& heightGrid)
{
	ChunkHeightRange range = {};

	range.minHeight = heightGrid.heights[0];
	range.maxHeight = heightGrid.heights[0];

	for (const WorldMetricCoordinate& height :
		heightGrid.heights)
	{
		if (compareWorldMetricCoordinates(
			height,
			range.minHeight) < 0)
		{
			range.minHeight = height;
		}

		if (compareWorldMetricCoordinates(
			height,
			range.maxHeight) > 0)
		{
			range.maxHeight = height;
		}
	}

	return range;
}
