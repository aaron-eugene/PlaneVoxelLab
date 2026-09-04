///////////////////////////////////////////////////////////////////////////////
// terrain_render/terrain_tile_atlas.cpp
// =====================================
//
// Implements shared terrain tile atlas UV mapping.
//
///////////////////////////////////////////////////////////////////////////////

#include "terrain_render/terrain_tile_atlas.h"

#include <glm/vec2.hpp>

#include <cassert>
#include <cstdint>

/***********************************************************
* Terrain Tile Atlas Interface
************************************************************/

glm::vec2 getTerrainAtlasUv(
	const glm::vec2& tileUv,
	TerrainTile tile)
{
	const uint32_t tileIndex =
		static_cast<uint32_t>(
			tile);

	assert(tileIndex < TERRAIN_TILE_COUNT);

	const float atlasWidth =
		static_cast<float>(
			TERRAIN_TILE_ATLAS_WIDTH_PIXELS);

	const float atlasHeight =
		static_cast<float>(
			TERRAIN_TILE_ATLAS_HEIGHT_PIXELS);

	const float tilePixelMinX =
		static_cast<float>(
			tileIndex *
			TERRAIN_TILE_SIZE_PIXELS);

	//--------------------------------------------------
	// Horizontal texel-center bounds
	//--------------------------------------------------

	const float tileMinU =
		(tilePixelMinX + 0.5f) /
		atlasWidth;

	const float tileMaxU =
		(tilePixelMinX +
			static_cast<float>(
				TERRAIN_TILE_SIZE_PIXELS) -
			0.5f) /
		atlasWidth;

	//--------------------------------------------------
	// Vertical texel-center bounds
	//--------------------------------------------------

	const float tileMinV =
		0.5f /
		atlasHeight;

	const float tileMaxV =
		(atlasHeight - 0.5f) /
		atlasHeight;

	return glm::vec2(
		tileMinU +
		(tileMaxU - tileMinU) *
		tileUv.x,
		tileMinV +
		(tileMaxV - tileMinV) *
		tileUv.y);
}
