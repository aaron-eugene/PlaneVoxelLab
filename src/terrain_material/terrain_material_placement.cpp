
///////////////////////////////////////////////////////////////////////////////
// terrain_material/terrain_material_placement.cpp
// ===============================================
//
// Implements terrain material classification from world
// elevation and deterministic boundary perturbation.
//
///////////////////////////////////////////////////////////////////////////////

#include "terrain_material/terrain_material_placement.h"

#include "math/noise.h"
#include "spatial/spatial_coordinates.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

/***********************************************************
* Terrain Material Constants
************************************************************/

static constexpr WorldMetricCoordinate
SAND_TO_GRASS_HEIGHT = { -8, 0.0f };

static constexpr WorldMetricCoordinate
GRASS_TO_ROCK_HEIGHT = { 0, 0.0f };

static constexpr WorldMetricCoordinate
ROCK_TO_SNOW_HEIGHT = { 12, 0.0f };

/***********************************************************
* Boundary Perturbation Constants
************************************************************/

static constexpr float BOUNDARY_PERTURBATION_AMPLITUDE = 1.5f;
static constexpr float BOUNDARY_INFLUENCE_DISTANCE = 4.0f;

static constexpr NoiseScale BOUNDARY_NOISE_SCALE = { 1, 12 };
static constexpr NoiseScale BOUNDARY_NOISE_LACUNARITY = { 2, 1 };

static constexpr uint32_t BOUNDARY_NOISE_OCTAVES = 3;
static constexpr float BOUNDARY_NOISE_PERSISTENCE = 0.5f;
static constexpr uint32_t BOUNDARY_NOISE_SEED = 1847;

/***********************************************************
* Grass Placement Constants
************************************************************/

static constexpr float GRASS_EXTENSION_BELOW = 2.0f;
static constexpr float GRASS_EXTENSION_ABOVE = 3.0f;

static constexpr float GRASS_MINIMUM_FLATNESS = 0.55f;
static constexpr float GRASS_FULL_FLATNESS = 0.90f;

static constexpr NoiseScale GRASS_COVERAGE_NOISE_SCALE = { 1, 5 };
static constexpr uint32_t GRASS_COVERAGE_NOISE_OCTAVES = 3;
static constexpr float GRASS_COVERAGE_NOISE_PERSISTENCE = 0.5f;
static constexpr uint32_t GRASS_COVERAGE_NOISE_SEED = 7351;

/***********************************************************
* Boundary Perturbation Helpers
************************************************************/

static float smoothBoundaryInfluence(
	float distance)
{
	const float t =
		std::clamp(
			1.0f - std::abs(distance) /
			BOUNDARY_INFLUENCE_DISTANCE,
			0.0f,
			1.0f);

	// Quintic smoothstep.
	return t * t * t *
		(t * (t * 6.0f - 15.0f) + 10.0f);
}

static float getBoundaryInfluence(
	const WorldMetricCoordinate& worldHeight,
	const WorldMetricCoordinate& boundaryHeight)
{
	// First establish a bounded local range so we never
	// convert a potentially enormous world coordinate
	// directly to float.

	const WorldMetricCoordinate minimumHeight =
		offsetWorldMetricCoordinate(
			boundaryHeight,
			-BOUNDARY_INFLUENCE_DISTANCE);

	const WorldMetricCoordinate maximumHeight =
		offsetWorldMetricCoordinate(
			boundaryHeight,
			BOUNDARY_INFLUENCE_DISTANCE);

	if (compareWorldMetricCoordinates(
		worldHeight, minimumHeight) <= 0 ||
		compareWorldMetricCoordinates(
			worldHeight, maximumHeight) >= 0)
	{
		return 0.0f;
	}

	// Safe here because the whole-meter difference
	// is bounded by the influence distance.

	const float distance =
		static_cast<float>(
			worldHeight.wholeMeters -
			boundaryHeight.wholeMeters) +
		worldHeight.fractionalMeter -
		boundaryHeight.fractionalMeter;

	return smoothBoundaryInfluence(distance);
}

static bool isBelowPerturbedBoundary(
	const WorldMetricCoordinate& worldHeight,
	const WorldMetricCoordinate& boundaryHeight,
	float noiseValue)
{
	const float influence =
		getBoundaryInfluence(
			worldHeight,
			boundaryHeight);

	if (influence <= 0.0f)
	{
		return compareWorldMetricCoordinates(
			worldHeight,
			boundaryHeight) < 0;
	}

	//--------------------------------------------------
	// Local Height Difference
	//--------------------------------------------------

	// getBoundaryInfluence() guarantees that the
	// position is within BOUNDARY_INFLUENCE_DISTANCE
	// meters of this boundary.

	const double relativeHeight =
		static_cast<double>(
			worldHeight.wholeMeters -
			boundaryHeight.wholeMeters) +
		static_cast<double>(
			worldHeight.fractionalMeter) -
		static_cast<double>(
			boundaryHeight.fractionalMeter);

	//--------------------------------------------------
	// Boundary Perturbation
	//--------------------------------------------------

	const double offset =
		static_cast<double>(noiseValue) *
		static_cast<double>(
			BOUNDARY_PERTURBATION_AMPLITUDE) *
		static_cast<double>(influence);

	return relativeHeight < offset;
}

static float sampleBoundaryNoise(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position)
{
	const WorldMetricCoordinate worldX =
		getWorldMetricCoordinate(
			chunkCoord.x,
			position.x);

	const WorldMetricCoordinate worldZ =
		getWorldMetricCoordinate(
			chunkCoord.z,
			position.z);

	return sampleFractalValueNoise2d(
		worldX.wholeMeters,
		worldZ.wholeMeters,
		worldX.fractionalMeter,
		worldZ.fractionalMeter,
		BOUNDARY_NOISE_SCALE,
		BOUNDARY_NOISE_OCTAVES,
		BOUNDARY_NOISE_PERSISTENCE,
		BOUNDARY_NOISE_LACUNARITY,
		BOUNDARY_NOISE_SEED);
}


/***********************************************************
* Grass Placement Helpers
************************************************************/

static float smoothMaterialTransition(
	float value)
{
	const float t =
		std::clamp(value, 0.0f, 1.0f);

	return t * t * t *
		(t * (t * 6.0f - 15.0f) + 10.0f);
}

static float sampleGrassCoverageNoise(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position)
{
	const WorldMetricCoordinate worldX =
		getWorldMetricCoordinate(
			chunkCoord.x,
			position.x);

	const WorldMetricCoordinate worldZ =
		getWorldMetricCoordinate(
			chunkCoord.z,
			position.z);

	const float noise =
		sampleFractalValueNoise2d(
			worldX.wholeMeters,
			worldZ.wholeMeters,
			worldX.fractionalMeter,
			worldZ.fractionalMeter,
			GRASS_COVERAGE_NOISE_SCALE,
			GRASS_COVERAGE_NOISE_OCTAVES,
			GRASS_COVERAGE_NOISE_PERSISTENCE,
			BOUNDARY_NOISE_LACUNARITY,
			GRASS_COVERAGE_NOISE_SEED);

	return std::clamp(
		noise * 0.5f + 0.5f,
		0.0f,
		1.0f);
}

static float getGrassElevationSuitability(
	const WorldMetricCoordinate& worldHeight,
	TerrainTile baselineTile)
{
	float distance = 0.0f;
	float extension = 0.0f;

	if (baselineTile == TerrainTile::Sand)
	{
		const WorldMetricCoordinate minimumHeight =
			offsetWorldMetricCoordinate(
				SAND_TO_GRASS_HEIGHT,
				-GRASS_EXTENSION_BELOW);

		if (compareWorldMetricCoordinates(
			worldHeight, minimumHeight) < 0 ||
			compareWorldMetricCoordinates(
				worldHeight, SAND_TO_GRASS_HEIGHT) >= 0)
		{
			return 0.0f;
		}

		distance = static_cast<float>(
			SAND_TO_GRASS_HEIGHT.wholeMeters -
			worldHeight.wholeMeters) +
			SAND_TO_GRASS_HEIGHT.fractionalMeter -
			worldHeight.fractionalMeter;

		extension = GRASS_EXTENSION_BELOW;
	}
	else if (baselineTile == TerrainTile::Rock)
	{
		const WorldMetricCoordinate maximumHeight =
			offsetWorldMetricCoordinate(
				GRASS_TO_ROCK_HEIGHT,
				GRASS_EXTENSION_ABOVE);

		if (compareWorldMetricCoordinates(
			worldHeight, GRASS_TO_ROCK_HEIGHT) < 0 ||
			compareWorldMetricCoordinates(
				worldHeight, maximumHeight) >= 0)
		{
			return 0.0f;
		}

		distance = static_cast<float>(
			worldHeight.wholeMeters -
			GRASS_TO_ROCK_HEIGHT.wholeMeters) +
			worldHeight.fractionalMeter -
			GRASS_TO_ROCK_HEIGHT.fractionalMeter;

		extension = GRASS_EXTENSION_ABOVE;
	}
	else
	{
		return 0.0f;
	}

	return 1.0f - smoothMaterialTransition(
		distance / extension);
}

static bool shouldExtendGrass(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position,
	const glm::vec3& normal,
	const WorldMetricCoordinate& worldHeight,
	TerrainTile baselineTile)
{
	const float elevationSuitability =
		getGrassElevationSuitability(
			worldHeight,
			baselineTile);

	if (elevationSuitability <= 0.0f)
	{
		return false;
	}

	const float flatnessSuitability =
		smoothMaterialTransition(
			(normal.y - GRASS_MINIMUM_FLATNESS) /
			(GRASS_FULL_FLATNESS -
				GRASS_MINIMUM_FLATNESS));

	if (flatnessSuitability <= 0.0f)
	{
		return false;
	}

	const float suitability =
		elevationSuitability *
		flatnessSuitability;

	const float coverage =
		sampleGrassCoverageNoise(
			chunkCoord,
			position);

	return coverage < suitability;
}

/***********************************************************
* Terrain Material Placement Interface
************************************************************/

TerrainTile classifyTerrainMaterial(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position,
	const glm::vec3& normal)
{
	const WorldMetricCoordinate worldHeight =
		getWorldMetricCoordinate(
			chunkCoord.y,
			position.y);

	const float noiseValue =
		sampleBoundaryNoise(
			chunkCoord,
			position);

	//--------------------------------------------------
	// Baseline Material Classification
	//--------------------------------------------------

	TerrainTile tile = TerrainTile::Snow;

	if (isBelowPerturbedBoundary(
		worldHeight,
		SAND_TO_GRASS_HEIGHT,
		noiseValue))
	{
		tile = TerrainTile::Sand;
	}
	else if (isBelowPerturbedBoundary(
		worldHeight,
		GRASS_TO_ROCK_HEIGHT,
		noiseValue))
	{
		tile = TerrainTile::Grass;
	}
	else if (isBelowPerturbedBoundary(
		worldHeight,
		ROCK_TO_SNOW_HEIGHT,
		noiseValue))
	{
		tile = TerrainTile::Rock;
	}

	//--------------------------------------------------
	// Active Material Placement
	//--------------------------------------------------

	if (shouldExtendGrass(
		chunkCoord,
		position,
		normal,
		worldHeight,
		tile))
	{
		tile = TerrainTile::Grass;
	}

	return tile;
}

