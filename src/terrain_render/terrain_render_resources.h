///////////////////////////////////////////////////////////////////////////////
// terrain_render/terrain_render_resources.h
// =========================================
//
// Defines shared rendering resources used by terrain experiments.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/texture_2d.h"

/***********************************************************
* Terrain Render Resource Types
************************************************************/

struct TerrainRenderResources
{
	Texture2D tileAtlas = {};
};

/***********************************************************
* Terrain Render Resource Interface
************************************************************/

bool createTerrainRenderResources(
	TerrainRenderResources& resources);

void destroyTerrainRenderResources(
	TerrainRenderResources& resources);
