///////////////////////////////////////////////////////////////////////////////
// lab_world.cpp
// =============
//
///////////////////////////////////////////////////////////////////////////////

#include "lab_world/lab_world.h"

#include "chunk/chunk.h"
#include "chunk/chunk_sampling.h"
#include "fields/density_field.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"
#include "surface/surface_map.h"

#include <cassert>
#include <cstdint>

/***********************************************************
* Lab World Lifecycle
************************************************************/

bool initializeLabWorld(
	LabWorld& world)
{
	assert(world.chunks.empty());

	world = {};

	world.sphereField.center = glm::dvec3(0.0, 0.0, 0.0);
	world.sphereField.radius =
		CHUNK_SIZE_METERS_D * 1.75;

	world.heightmapField.baseHeight = 0.0;
	world.heightmapField.amplitude = 8.0;
	world.heightmapField.frequency = 0.08;

	setLabWorldDensityFieldType(
		world,
		LabWorldDensityFieldType::Sphere);

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
				Chunk chunk = {};

				const ChunkCoord coord =
				{
					chunkX,
					chunkY,
					chunkZ
				};

				if (!initializeChunk(
					chunk,
					coord))
				{
					world = {};
					return false;
				}

				sampleChunkDensityField(
					chunk,
					world.densityField);

				world.chunks.push_back(chunk);
			}
		}
	}

	assert(world.chunks.size() == CHUNK_LOAD_COUNT);

	if (!rebuildSurfaceMap(
		world.surfaceMap,
		world.chunks))
	{
		world = {};
		return false;
	}

	return true;
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
			createSphereDensityField(world.sphereField);
	} break;

	case LabWorldDensityFieldType::Heightmap:
	{
		world.densityField =
			createHeightmapDensityField(world.heightmapField);
	} break;

	default:
	{
		assert(false);
		world.densityField = {};
	} break;
	}

	assert(isDensityFieldValid(world.densityField));
}

bool rebuildLabWorldDensityData(
	LabWorld& world)
{
	assert(isDensityFieldValid(world.densityField));

	for (Chunk& chunk : world.chunks)
	{
		sampleChunkDensityField(
			chunk,
			world.densityField);
	}

	if (!rebuildSurfaceMap(
		world.surfaceMap,
		world.chunks))
	{
		clearSurfaceMap(world.surfaceMap);
		return false;
	}

	return true;
}
