///////////////////////////////////////////////////////////////////////////////
// lab/terrain_render_resources.cpp
// ================================
//
// Implements shared terrain rendering resource creation and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/terrain_render_resources.h"

#include <array>
#include <cassert>
#include <cstdint>

/***********************************************************
* Terrain Render Resource Interface
************************************************************/

bool createTerrainRenderResources(
	TerrainRenderResources& resources)
{
	assert(resources.tileAtlas.handle == 0);
	assert(resources.tileAtlas.width == 0);
	assert(resources.tileAtlas.height == 0);

	//--------------------------------------------------
	// Temporary 2 x 2 test atlas
	//--------------------------------------------------

	constexpr uint32_t atlasWidth = 2;
	constexpr uint32_t atlasHeight = 2;

	constexpr std::array<uint8_t, atlasWidth* atlasHeight * 4>
		atlasPixels =
	{
		// Bottom-left
		255, 0, 255, 255,

		// Bottom-right
		255, 255, 255, 255,

		// Top-left
		0, 0, 0, 255,

		// Top-right
		0, 255, 255, 255,
	};

	Texture2DCreateInfo createInfo = {};

	createInfo.width = atlasWidth;
	createInfo.height = atlasHeight;
	createInfo.rgbaPixels = atlasPixels.data();

	createInfo.minFilter = TextureFilter::Nearest;
	createInfo.magFilter = TextureFilter::Nearest;

	createInfo.wrapS = TextureWrap::ClampToEdge;
	createInfo.wrapT = TextureWrap::ClampToEdge;

	if (!createTexture2D(
		resources.tileAtlas,
		createInfo))
	{
		destroyTerrainRenderResources(resources);
		return false;
	}

	return true;
}

void destroyTerrainRenderResources(
	TerrainRenderResources& resources)
{
	destroyTexture2D(resources.tileAtlas);

	resources = {};
}
