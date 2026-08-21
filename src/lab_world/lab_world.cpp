///////////////////////////////////////////////////////////////////////////////
// lab_world.cpp
// =============
// Implements LabWorld initialization, density-field selection, and rebuilding.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab_world/lab_world.h"

#include "chunk/chunk.h"
#include "chunk/chunk_sampling.h"
#include "fields/density_field.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world_constants.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"
#include "surface_map/surface_map.h"

#include <cassert>
#include <cstdint>

/***********************************************************
* Lab World Lifecycle
************************************************************/

void initializeLabWorld(
	LabWorld& world)
{
	world = {};

	world.sphereField.center = glm::dvec3(0.0, 0.0, 0.0);
	world.sphereField.radius =
		CHUNK_SIZE_METERS_D * 1.75;

	world.heightmapField.baseHeight = 0.0f;
	world.heightmapField.amplitude = 22.0f;
	world.heightmapField.frequency = 0.04f;

	setLabWorldDensityFieldType(
		world,
		LabWorldDensityFieldType::Heightmap);

	world.chunks.reserve(CHUNK_LOAD_COUNT);

	for (int32_t chunkZ = -CHUNK_LOAD_RADIUS_Z;
		chunkZ < CHUNK_LOAD_RADIUS_Z;
		++chunkZ)
	{
		for (int32_t chunkY = -CHUNK_LOAD_RADIUS_Y;
			chunkY < CHUNK_LOAD_RADIUS_Y;
			++chunkY)
		{
			for (int32_t chunkX = -CHUNK_LOAD_RADIUS_X;
				chunkX < CHUNK_LOAD_RADIUS_X;
				++chunkX)
			{
				const ChunkCoord coord =
				{
					chunkX,
					chunkY,
					chunkZ
				};

				world.chunks.emplace_back();

				Chunk& chunk = world.chunks.back();

				initializeChunk(
					chunk,
					coord);

				sampleChunkDensityField(
					chunk,
					world.densityField);
			}
		}
	}

	assert(world.chunks.size() == CHUNK_LOAD_COUNT);

	rebuildSurfaceMap(
		world.surfaceMap,
		world.chunks);
}

void shutdownLabWorld(LabWorld& world)
{
	world = {};
}

/***********************************************************
* Lab World Density Field
************************************************************/

void setLabWorldDensityFieldType(
	LabWorld& world,
	LabWorldDensityFieldType fieldType)
{
	world.activeDensityFieldType = fieldType;

	switch (world.activeDensityFieldType)
	{
	case LabWorldDensityFieldType::Sphere:
	{
		world.densityField =
			makeSphereDensityField(world.sphereField);
	} break;

	case LabWorldDensityFieldType::Heightmap:
	{
		world.densityField =
			makeHeightmapDensityField(world.heightmapField);
	} break;

	default:
	{
		assert(false);
		world.densityField = {};
	} break;
	}

	assert(isDensityFieldValid(world.densityField));
}

void rebuildLabWorldDensityData(
	LabWorld& world)
{
	assert(isDensityFieldValid(world.densityField));

	for (Chunk& chunk : world.chunks)
	{
		sampleChunkDensityField(
			chunk,
			world.densityField);
	}

	rebuildSurfaceMap(
		world.surfaceMap,
		world.chunks);
}
