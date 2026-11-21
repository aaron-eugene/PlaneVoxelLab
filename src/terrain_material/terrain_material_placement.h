
///////////////////////////////////////////////////////////////////////////////
// terrain_material/terrain_material_placement.h
// =============================================
//
// Declares terrain material classification based on world
// elevation and surface orientation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"
#include "terrain_render/terrain_tile_atlas.h"

#include <glm/fwd.hpp>

/***********************************************************
* Terrain Material Placement Interface
************************************************************/

TerrainTile classifyTerrainMaterial(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position,
	const glm::vec3& normal);
