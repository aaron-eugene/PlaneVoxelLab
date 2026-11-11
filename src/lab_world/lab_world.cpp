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
#include <utility>
#include <vector>

// TEST
#include <chrono>
#include <cstdio>

/***********************************************************
* Lab World Chunk Initialization
************************************************************/

static void initializeLabWorldChunks(
	LabWorld& world)
{
	world.chunks.reserve(
		CHUNK_LOAD_COUNT);

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

				Chunk& chunk =
					world.chunks.back();

				initializeChunk(
					chunk,
					coord);
			}
		}
	}

	assert(
		world.chunks.size() ==
		CHUNK_LOAD_COUNT);
}

/***********************************************************
* Lab World Density Sampling
************************************************************/

static void sampleLabWorldGeneralDensity(
	LabWorld& world)
{
	for (Chunk& chunk : world.chunks)
	{
		sampleChunkDensityField(
			chunk,
			world.densityField);
	}
}

// Samples the complete fixed lab region.
// Requires all configured chunk coordinates to be initialized.
static void sampleLabWorldHeightmapDensity(
	LabWorld& world)
{
	for (int32_t chunkZ = -CHUNK_LOAD_RADIUS_Z;
		chunkZ < CHUNK_LOAD_RADIUS_Z;
		++chunkZ)
	{
		for (int32_t chunkX = -CHUNK_LOAD_RADIUS_X;
			chunkX < CHUNK_LOAD_RADIUS_X;
			++chunkX)
		{
			const ChunkCoord columnCoord =
			{
				chunkX,
				0,
				chunkZ
			};

			ChunkHeightGrid heightGrid = {};

			sampleChunkHeightGrid(
				heightGrid,
				columnCoord,
				world.heightmapField);

			for (int32_t chunkY = -CHUNK_LOAD_RADIUS_Y;
				chunkY < CHUNK_LOAD_RADIUS_Y;
				++chunkY)
			{
				const ChunkCoord chunkCoord =
				{
					chunkX,
					chunkY,
					chunkZ
				};

				Chunk* chunk =
					findLabWorldChunk(
						world,
						chunkCoord);

				assert(chunk != nullptr);

				populateChunkDensityFromHeightGrid(
					*chunk,
					heightGrid);
			}
		}
	}
}

static void sampleLabWorldDensity(
	LabWorld& world)
{
	switch (world.activeDensityFieldType)
	{
	case LabWorldDensityFieldType::Sphere:
	{
		sampleLabWorldGeneralDensity(
			world);
	} break;

	case LabWorldDensityFieldType::Heightmap:
	{
		sampleLabWorldHeightmapDensity(
			world);
	} break;

	default:
	{
		assert(false);
	} break;
	}
}

/***********************************************************
* Lab World Surface Discovery
************************************************************/

static void rebuildLabWorldSurfaceMap(
	LabWorld& world)
{
	std::vector<DiscoveredSurfaceChunk> discoveredChunks = {};

	switch (world.activeDensityFieldType)
	{
	case LabWorldDensityFieldType::Sphere:
	{
		discoverSurfaceChunks(
			discoveredChunks,
			world.chunks);
	} break;

	case LabWorldDensityFieldType::Heightmap:
	{
		discoverHeightmapSurfaceChunks(
			discoveredChunks,
			world.chunks,
			world.heightmapField);
	} break;

	default:
	{
		assert(false);
	} break;
	}

	rebuildSurfaceMap(
		world.surfaceMap,
		std::move(discoveredChunks));
}

/***********************************************************
* Lab World Lifecycle
************************************************************/

void initializeLabWorld(
	LabWorld& world)
{
	using Clock = std::chrono::steady_clock;

	const auto worldStart = Clock::now();

	world = {};

	world.sphereField.center = {};
	world.sphereField.radius =
		CHUNK_SIZE_METERS * 1.75f;

	world.heightmapField.baseHeight = {};
	world.heightmapField.amplitude = 22.0f;
	world.heightmapField.baseScale = { 1, 25 };

	setLabWorldDensityFieldType(
		world,
		LabWorldDensityFieldType::Sphere);

	initializeLabWorldChunks(
		world);

	const auto samplingStart = Clock::now();

	sampleLabWorldDensity(
		world);

	const auto samplingStop = Clock::now();

	rebuildLabWorldSurfaceMap(
		world);

	const auto surfaceMapStop = Clock::now();

	const double samplingMs =
		std::chrono::duration<double, std::milli>(
			samplingStop - samplingStart).count();

	const double surfaceMapMs =
		std::chrono::duration<double, std::milli>(
			surfaceMapStop - samplingStop).count();

	const double totalMs =
		std::chrono::duration<double, std::milli>(
			surfaceMapStop - worldStart).count();

	std::printf(
		"LabWorld initialization:\n"
		"  Density sampling:        %.2f ms\n"
		"  Surface discovery + map: %.2f ms\n"
		"  Total:                   %.2f ms\n",
		samplingMs,
		surfaceMapMs,
		totalMs);
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
	assert(
		isDensityFieldValid(
			world.densityField));

	sampleLabWorldDensity(
		world);

	rebuildLabWorldSurfaceMap(
		world);
}

/***********************************************************
* Lab World Chunk Lookup
************************************************************/

Chunk* findLabWorldChunk(
	LabWorld& world,
	const ChunkCoord& coord)
{
	for (Chunk& chunk : world.chunks)
	{
		if (chunk.coord.x == coord.x &&
			chunk.coord.y == coord.y &&
			chunk.coord.z == coord.z)
		{
			return &chunk;
		}
	}

	return nullptr;
}

const Chunk* findLabWorldChunk(
	const LabWorld& world,
	const ChunkCoord& coord)
{
	for (const Chunk& chunk : world.chunks)
	{
		if (chunk.coord.x == coord.x &&
			chunk.coord.y == coord.y &&
			chunk.coord.z == coord.z)
		{
			return &chunk;
		}
	}

	return nullptr;
}
