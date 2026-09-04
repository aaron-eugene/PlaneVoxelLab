///////////////////////////////////////////////////////////////////////////////
// terrain_render/terrain_tile_atlas.h
// ===================================
//
// Defines the shared terrain tile vocabulary, atlas layout, and UV mapping.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/fwd.hpp>

#include <cstdint>

/***********************************************************
* Terrain Tile Atlas Types
************************************************************/

enum class TerrainTile
{
	Grass = 0,
	Sand,
	Rock,
	Snow,

	Count,
};

/***********************************************************
* Terrain Tile Atlas Constants
************************************************************/

constexpr uint32_t TERRAIN_TILE_SIZE_PIXELS = 256;

constexpr uint32_t TERRAIN_TILE_COUNT =
	static_cast<uint32_t>(TerrainTile::Count);

constexpr uint32_t TERRAIN_TILE_ATLAS_WIDTH_PIXELS =
	TERRAIN_TILE_SIZE_PIXELS *
	TERRAIN_TILE_COUNT;

constexpr uint32_t TERRAIN_TILE_ATLAS_HEIGHT_PIXELS =
	TERRAIN_TILE_SIZE_PIXELS;

/***********************************************************
* Terrain Tile Atlas Interface
************************************************************/

glm::vec2 getTerrainAtlasUv(
	const glm::vec2& tileUv,
	TerrainTile tile);
