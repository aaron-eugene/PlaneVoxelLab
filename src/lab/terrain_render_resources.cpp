///////////////////////////////////////////////////////////////////////////////
// lab/terrain_render_resources.cpp
// ================================
//
// Implements shared terrain rendering resource creation and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/terrain_render_resources.h"

#include "renderer/image_data.h"
#include "lab/terrain_tile_atlas.h"

#include <cassert>

/***********************************************************
* Terrain Render Resource Constants
************************************************************/

static constexpr const char* TERRAIN_TILE_ATLAS_PATH =
"assets/textures/terrain_tile_atlas.png";

/***********************************************************
* Terrain Render Resource Interface
************************************************************/

bool createTerrainRenderResources(
	TerrainRenderResources& resources)
{
	assert(resources.tileAtlas.handle == 0);
	assert(resources.tileAtlas.width == 0);
	assert(resources.tileAtlas.height == 0);

	ImageData atlasImage = {};

	if (!loadImageData(
		atlasImage,
		TERRAIN_TILE_ATLAS_PATH))
	{
		return false;
	}

	if (atlasImage.width !=
		TERRAIN_TILE_ATLAS_WIDTH_PIXELS ||
		atlasImage.height !=
		TERRAIN_TILE_ATLAS_HEIGHT_PIXELS)
	{
		destroyImageData(
			atlasImage);

		return false;
	}

	Texture2DCreateInfo createInfo = {};

	createInfo.width =
		atlasImage.width;

	createInfo.height =
		atlasImage.height;

	createInfo.rgbaPixels =
		atlasImage.rgbaPixels;

	createInfo.minFilter =
		TextureFilter::Nearest;

	createInfo.magFilter =
		TextureFilter::Nearest;

	createInfo.wrapS =
		TextureWrap::ClampToEdge;

	createInfo.wrapT =
		TextureWrap::ClampToEdge;

	const bool textureCreated =
		createTexture2D(
			resources.tileAtlas,
			createInfo);

	destroyImageData(
		atlasImage);

	if (!textureCreated)
	{
		destroyTerrainRenderResources(
			resources);

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
