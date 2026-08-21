///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.cpp
// ===============================================
//
// Implements CPU-side mesh construction for the XZ columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_builder.h"

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "experiments/xz_columnar/xz_columnar_planar_cell.h"
#include "experiments/xz_columnar/xz_columnar_side_region.h"
#include "fields/field_generators.h"
#include "lab/terrain_tile_atlas.h"
#include "renderer/render_vertex.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"
#include "surface_map/surface_map.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

/***********************************************************
* Y-Range Helpers
************************************************************/

static uint32_t getClampedLocalVoxelYFromWorldY(
	float worldY,
	float chunkMinY)
{
	const float localY =
		(worldY - chunkMinY) /
		VOXEL_SIZE_METERS;

	const int32_t unclampedVoxelY =
		static_cast<int32_t>(
			std::floor(localY));

	const int32_t clampedVoxelY =
		std::max(
			0,
			std::min(
				static_cast<int32_t>(CHUNK_SIZE - 1),
				unclampedVoxelY));

	return static_cast<uint32_t>(clampedVoxelY);
}

/***********************************************************
* Texturing Helpers
************************************************************/

static glm::vec2 getXZColumnarTopTileUv(
	const glm::vec3& worldPosition,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel)
{
	const float cellWorldMinX =
		chunkWorldMin.x +
		static_cast<float>(ownerVoxel.x) *
		VOXEL_SIZE_METERS;

	const float cellWorldMinZ =
		chunkWorldMin.z +
		static_cast<float>(ownerVoxel.z) *
		VOXEL_SIZE_METERS;

	return glm::vec2(
		(worldPosition.x - cellWorldMinX) /
		VOXEL_SIZE_METERS,
		(worldPosition.z - cellWorldMinZ) /
		VOXEL_SIZE_METERS);
}

static glm::vec2 getXZColumnarSideTileUv(
	const glm::vec3& worldPosition,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel,
	XZColumnarSide side)
{
	const float voxelWorldMinX =
		chunkWorldMin.x +
		static_cast<float>(ownerVoxel.x) *
		VOXEL_SIZE_METERS;

	const float voxelWorldMinY =
		chunkWorldMin.y +
		static_cast<float>(ownerVoxel.y) *
		VOXEL_SIZE_METERS;

	const float voxelWorldMinZ =
		chunkWorldMin.z +
		static_cast<float>(ownerVoxel.z) *
		VOXEL_SIZE_METERS;

	float tileU = 0.0f;

	switch (side)
	{
	case XZColumnarSide::NegativeX:
	{
		tileU =
			1.0f -
			(worldPosition.z - voxelWorldMinZ) /
			VOXEL_SIZE_METERS;
	} break;

	case XZColumnarSide::PositiveX:
	{
		tileU =
			(worldPosition.z - voxelWorldMinZ) /
			VOXEL_SIZE_METERS;
	} break;

	case XZColumnarSide::NegativeZ:
	{
		tileU =
			(worldPosition.x - voxelWorldMinX) /
			VOXEL_SIZE_METERS;
	} break;

	case XZColumnarSide::PositiveZ:
	{
		tileU =
			1.0f -
			(worldPosition.x - voxelWorldMinX) /
			VOXEL_SIZE_METERS;
	} break;

	default:
	{
		assert(false);
	} break;
	}

	const float tileV =
		(worldPosition.y - voxelWorldMinY) /
		VOXEL_SIZE_METERS;

	return glm::vec2(
		tileU,
		tileV);
}

/***********************************************************
* Mesh Emission Helpers
************************************************************/

static uint32_t appendStandardVertexToMesh(
	XZColumnarMesh& mesh,
	const glm::vec3& worldPosition,
	const glm::vec3& chunkWorldMin,
	const glm::vec3& normal,
	const glm::vec3& color,
	const glm::vec2& tileUv)
{
	StandardVertex vertex = {};

	vertex.position = worldPosition -
		chunkWorldMin;

	vertex.normal = normal;
	vertex.color = color;
	vertex.tileUv = tileUv;

	const uint32_t vertexIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	mesh.vertices.push_back(vertex);

	return vertexIndex;
}

static void appendTriangleToMesh(
	XZColumnarMesh& mesh,
	uint32_t index0,
	uint32_t index1,
	uint32_t index2)
{
	mesh.indices.push_back(index0);
	mesh.indices.push_back(index1);
	mesh.indices.push_back(index2);
}

static void emitVoxelOwnedTopPiece(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const glm::vec3& normal,
	const VoxelCoord& ownerVoxel,
	TerrainTile terrainTile,
	const XZColumnarBuildSettings& settings)
{
	if (polygon.vertexCount < 3)
	{
		return;
	}

	XZColumnarTopPiece topPiece = {};
	topPiece.ownerVoxel = ownerVoxel;
	topPiece.firstIndex =
		static_cast<uint32_t>(
			mesh.indices.size());

	// Temporary neutral color while verifying textures
	(void)settings;
	const glm::vec3 pieceColor =
		glm::vec3(1.0f);

	const uint32_t baseVertexIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& clipVertex =
			polygon.vertices[vertexIndex];

		const glm::vec2 tileUv =
			getXZColumnarTopTileUv(
				clipVertex.position,
				chunkWorldMin,
				ownerVoxel);

		const glm::vec2 atlasUv =
			getTerrainAtlasUv(
				tileUv,
				terrainTile);

		appendStandardVertexToMesh(
			mesh,
			clipVertex.position,
			chunkWorldMin,
			normal,
			pieceColor,
			atlasUv);
	}

	for (uint32_t vertexIndex = 1;
		vertexIndex + 1 < polygon.vertexCount;
		++vertexIndex)
	{
		appendTriangleToMesh(
			mesh,
			baseVertexIndex,
			baseVertexIndex + vertexIndex,
			baseVertexIndex + vertexIndex + 1);
	}

	topPiece.indexCount =
		static_cast<uint32_t>(
			mesh.indices.size()) -
		topPiece.firstIndex;

	if (topPiece.indexCount > 0)
	{
		mesh.topPieces.push_back(
			topPiece);
	}
}

static bool sliceAndEmitTopPolygonByVoxelY(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const glm::vec3& normal,
	uint32_t localX,
	uint32_t localZ,
	TerrainTile terrainTile,
	const XZColumnarBuildSettings& settings)
{
	bool emittedTopPiece = false;
	
	if (polygon.vertexCount < 3)
	{
		return emittedTopPiece;
	}

	const float chunkMinY =
		chunkWorldMin.y;

	const float chunkMaxY =
		chunkWorldMin.y + CHUNK_SIZE_METERS_F;

	const XZColumnarClipPolygon chunkClippedPolygon =
		clipXZColumnarPolygonToYSlab(
			polygon,
			chunkMinY,
			chunkMaxY);

	if (chunkClippedPolygon.vertexCount < 3)
	{
		return emittedTopPiece;
	}

	const float polygonMinY =
		getXZColumnarPolygonMinY(chunkClippedPolygon);

	const float polygonMaxY =
		getXZColumnarPolygonMaxY(chunkClippedPolygon);

	const uint32_t firstLocalY =
		getClampedLocalVoxelYFromWorldY(
			polygonMinY,
			chunkMinY);

	const uint32_t lastLocalY =
		getClampedLocalVoxelYFromWorldY(
			polygonMaxY,
			chunkMinY);

	for (uint32_t localY = firstLocalY;
		localY <= lastLocalY;
		++localY)
	{
		const float voxelMinY =
			chunkMinY +
			static_cast<float>(localY) *
			VOXEL_SIZE_METERS;

		const float voxelMaxY =
			voxelMinY + VOXEL_SIZE_METERS;

		const XZColumnarClipPolygon voxelClippedPolygon =
			clipXZColumnarPolygonToYSlab(
				chunkClippedPolygon,
				voxelMinY,
				voxelMaxY);

		if (voxelClippedPolygon.vertexCount < 3)
		{
			continue;
		}

		VoxelCoord ownerVoxel = {};
		ownerVoxel.x = localX;
		ownerVoxel.y = localY;
		ownerVoxel.z = localZ;

		emitVoxelOwnedTopPiece(
			mesh,
			voxelClippedPolygon,
			chunkWorldMin,
			normal,
			ownerVoxel,
			terrainTile,
			settings);

		emittedTopPiece = true;
	}

	return emittedTopPiece;
}

static void emitVoxelOwnedSideFragment(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel,
	XZColumnarSide side,
	TerrainTile terrainTile,
	const XZColumnarBuildSettings& settings)
{
	if (polygon.vertexCount < 3)
	{
		return;
	}

	XZColumnarSideFragment fragment = {};
	fragment.ownerVoxel = ownerVoxel;
	fragment.side = side;
	fragment.firstIndex =
		static_cast<uint32_t>(
			mesh.indices.size());

	const glm::vec3 fragmentNormal =
		getXZColumnarSideNormal(
			side);

	// Temporary neutral color while verifying textures.
	(void)settings;
	const glm::vec3 fragmentColor =
		glm::vec3(1.0f);

	const uint32_t baseVertexIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		const glm::vec3& worldPosition =
			polygon.vertices[
				vertexIndex].position;

		const glm::vec2 tileUv =
			getXZColumnarSideTileUv(
				worldPosition,
				chunkWorldMin,
				ownerVoxel,
				side);

		const glm::vec2 atlasUv =
			getTerrainAtlasUv(
				tileUv,
				terrainTile);

		appendStandardVertexToMesh(
			mesh,
			worldPosition,
			chunkWorldMin,
			fragmentNormal,
			fragmentColor,
			atlasUv);
	}

	for (uint32_t vertexIndex = 1;
		vertexIndex + 1 < polygon.vertexCount;
		++vertexIndex)
	{
		appendTriangleToMesh(
			mesh,
			baseVertexIndex,
			baseVertexIndex + vertexIndex,
			baseVertexIndex + vertexIndex + 1);
	}

	fragment.indexCount =
		static_cast<uint32_t>(
			mesh.indices.size()) -
		fragment.firstIndex;

	if (fragment.indexCount > 0)
	{
		mesh.sideFragments.push_back(
			fragment);
	}
}

static void sliceAndEmitOwnedSideRegion(
	XZColumnarMesh& mesh,
	const XZColumnarSideRegion& region,
	const glm::vec3& chunkWorldMin,
	TerrainTile terrainTile,
	const XZColumnarBuildSettings& settings)
{
	const XZColumnarClipPolygon sidePolygon =
		getXZColumnarSideRegionPolygon(
			region);

	if (sidePolygon.vertexCount < 3)
	{
		return;
	}

	const float chunkMinY =
		chunkWorldMin.y;

	const float chunkMaxY =
		chunkMinY +
		CHUNK_SIZE_METERS_F;

	const XZColumnarClipPolygon chunkClippedPolygon =
		clipXZColumnarPolygonToYSlab(
			sidePolygon,
			chunkMinY,
			chunkMaxY);

	if (chunkClippedPolygon.vertexCount < 3)
	{
		return;
	}

	const uint32_t firstLocalY =
		getClampedLocalVoxelYFromWorldY(
			getXZColumnarPolygonMinY(
				chunkClippedPolygon),
			chunkMinY);

	const uint32_t lastLocalY =
		getClampedLocalVoxelYFromWorldY(
			getXZColumnarPolygonMaxY(
				chunkClippedPolygon),
			chunkMinY);

	for (uint32_t localY = firstLocalY;
		localY <= lastLocalY;
		++localY)
	{
		const float voxelMinY =
			chunkMinY +
			static_cast<float>(localY) *
			VOXEL_SIZE_METERS;

		const float voxelMaxY =
			voxelMinY +
			VOXEL_SIZE_METERS;

		const XZColumnarClipPolygon
			voxelClippedPolygon =
			clipXZColumnarPolygonToYSlab(
				chunkClippedPolygon,
				voxelMinY,
				voxelMaxY);

		if (voxelClippedPolygon.vertexCount < 3)
		{
			continue;
		}

		VoxelCoord ownerVoxel = {};
		ownerVoxel.x =
			static_cast<uint32_t>(
				region.ownerRelativeX);

		ownerVoxel.y =
			localY;

		ownerVoxel.z =
			static_cast<uint32_t>(
				region.ownerRelativeZ);
		
		emitVoxelOwnedSideFragment(
			mesh,
			voxelClippedPolygon,
			chunkWorldMin,
			ownerVoxel,
			region.ownerSide,
			terrainTile,
			settings);
	}
}

/***********************************************************
* Surface Chunk Mesh Construction
************************************************************/

static bool buildXZColumnarMeshForSurfaceChunk(
	XZColumnarMesh& mesh,
	const XZColumnarPlanarCellGrid& grid,
	const SurfaceChunk& surfaceChunk,
	const XZColumnarBuildSettings& settings)
{	
	mesh = {};
	mesh.coord = surfaceChunk.coord;

	const glm::dvec3 chunkWorldMinD =
		getChunkWorldMin(
			surfaceChunk.coord);

	const glm::vec3 chunkWorldMin =
		glm::vec3(chunkWorldMinD);

	//--------------------------------------------------
	// Emit Owned Top Pieces
	//--------------------------------------------------
	for (uint32_t localZ = 0;
		localZ < CHUNK_SIZE;
		++localZ)
	{
		for (uint32_t localX = 0;
			localX < CHUNK_SIZE;
			++localX)
		{
			const XZColumnarPlanarCell& planarCell =
				getXZColumnarPlanarCell(
					grid,
					static_cast<int32_t>(localX),
					static_cast<int32_t>(localZ));

			const XZColumnarClipPolygon topPolygon =
				getXZColumnarPlanarCellTopPolygon(
					planarCell);

			const bool emittedTopPiece =
				sliceAndEmitTopPolygonByVoxelY(
					mesh,
					topPolygon,
					chunkWorldMin,
					planarCell.normal,
					localX,
					localZ,
					planarCell.surfaceTile,
					settings);

			// Aggregate height range
			if (emittedTopPiece)
			{
				if (!mesh.hasSurfaceHeightRange)
				{
					mesh.minSurfaceHeightMeters =
						planarCell.surfaceHeightMeters;

					mesh.maxSurfaceHeightMeters =
						planarCell.surfaceHeightMeters;

					mesh.hasSurfaceHeightRange = true;
				}
				else
				{
					mesh.minSurfaceHeightMeters =
						std::min(
							mesh.minSurfaceHeightMeters,
							planarCell.surfaceHeightMeters);

					mesh.maxSurfaceHeightMeters =
						std::max(
							mesh.maxSurfaceHeightMeters,
							planarCell.surfaceHeightMeters);
				}
			}
		}
	}

	//--------------------------------------------------
	// Discover X Side Regions
	//--------------------------------------------------
	for (int32_t relativeZ = 0;
		relativeZ < static_cast<int32_t>(CHUNK_SIZE);
		++relativeZ)
	{
		for (int32_t negativeX = -1;
			negativeX < static_cast<int32_t>(CHUNK_SIZE);
			++negativeX)
		{
			const int32_t positiveX =
				negativeX + 1;

			const XZColumnarPlanarCell& negativeXCell =
				getXZColumnarPlanarCell(
					grid,
					negativeX,
					relativeZ);

			const XZColumnarPlanarCell& positiveXCell =
				getXZColumnarPlanarCell(
					grid,
					positiveX,
					relativeZ);

			XZColumnarSideRegion regions[2] = {};

			const uint32_t regionCount =
				buildXZColumnarSharedEdgeSideRegions(
					regions,
					negativeXCell,
					XZColumnarSide::PositiveX,
					positiveXCell,
					XZColumnarSide::NegativeX);

			for (uint32_t regionIndex = 0;
				regionIndex < regionCount;
				++regionIndex)
			{
				const XZColumnarSideRegion& region =
					regions[regionIndex];

				if (!isXZColumnarPlanarCellCoordinateOwned(
					region.ownerRelativeX,
					region.ownerRelativeZ))
				{
					continue;
				}

				const XZColumnarPlanarCell& ownerCell =
					getXZColumnarPlanarCell(
						grid,
						region.ownerRelativeX,
						region.ownerRelativeZ);

				sliceAndEmitOwnedSideRegion(
					mesh,
					region,
					chunkWorldMin,
					ownerCell.surfaceTile,
					settings);
			}
		}
	}

	//--------------------------------------------------
	// Discover Z Side Regions
	//--------------------------------------------------
	for (int32_t relativeX = 0;
		relativeX < static_cast<int32_t>(CHUNK_SIZE);
		++relativeX)
	{
		for (int32_t negativeZ = -1;
			negativeZ < static_cast<int32_t>(CHUNK_SIZE);
			++negativeZ)
		{
			const int32_t positiveZ =
				negativeZ + 1;

			const XZColumnarPlanarCell& negativeZCell =
				getXZColumnarPlanarCell(
					grid,
					relativeX,
					negativeZ);

			const XZColumnarPlanarCell& positiveZCell =
				getXZColumnarPlanarCell(
					grid,
					relativeX,
					positiveZ);

			XZColumnarSideRegion regions[2] = {};

			const uint32_t regionCount =
				buildXZColumnarSharedEdgeSideRegions(
					regions,
					negativeZCell,
					XZColumnarSide::PositiveZ,
					positiveZCell,
					XZColumnarSide::NegativeZ);

			for (uint32_t regionIndex = 0;
				regionIndex < regionCount;
				++regionIndex)
			{
				const XZColumnarSideRegion& region =
					regions[regionIndex];

				if (!isXZColumnarPlanarCellCoordinateOwned(
					region.ownerRelativeX,
					region.ownerRelativeZ))
				{
					continue;
				}

				const XZColumnarPlanarCell& ownerCell =
					getXZColumnarPlanarCell(
						grid,
						region.ownerRelativeX,
						region.ownerRelativeZ);

				sliceAndEmitOwnedSideRegion(
					mesh,
					region,
					chunkWorldMin,
					ownerCell.surfaceTile,
					settings);
			}
		}
	}

	return true;
}

/***********************************************************
* Columnar Patch Mesh Lifecycle
************************************************************/

void clearXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes)
{
	meshes.clear();
}

bool buildXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes,
	const HeightmapDensityField& heightmap,
	const SurfaceMap& surfaceMap,
	const XZColumnarBuildSettings& settings)
{
	assert(settings.derivativeStepMeters > 0.0f);

	clearXZColumnarMeshes(meshes);

	meshes.reserve(
		surfaceMap.chunks.size());

	XZColumnarPlanarCellGrid planarCellGrid = {};

	initializeXZColumnarPlanarCellGrid(
		planarCellGrid);

	uint32_t expectedSurfaceChunkIndex = 0;

	for (const SurfaceChunkColumn& column :
		surfaceMap.columns)
	{
		assert(column.surfaceChunkCount > 0);

		assert(
			column.firstSurfaceChunkIndex ==
			expectedSurfaceChunkIndex);

		const uint32_t columnEndIndex =
			column.firstSurfaceChunkIndex +
			column.surfaceChunkCount;

		assert(
			columnEndIndex <=
			surfaceMap.chunks.size());

		const SurfaceChunk& firstSurfaceChunk =
			surfaceMap.chunks[
				column.firstSurfaceChunkIndex];

		assert(
			firstSurfaceChunk.coord.x ==
			column.chunkX);

		assert(
			firstSurfaceChunk.coord.z ==
			column.chunkZ);

		buildXZColumnarPlanarCellGrid(
			planarCellGrid,
			heightmap,
			column.chunkX,
			column.chunkZ,
			settings.derivativeStepMeters);

		for (uint32_t surfaceChunkIndex =
			column.firstSurfaceChunkIndex;
			surfaceChunkIndex < columnEndIndex;
			++surfaceChunkIndex)
		{
			const SurfaceChunk& surfaceChunk =
				surfaceMap.chunks[
					surfaceChunkIndex];

			assert(
				surfaceChunk.coord.x ==
				column.chunkX);

			assert(
				surfaceChunk.coord.z ==
				column.chunkZ);

			XZColumnarMesh mesh = {};

			if (!buildXZColumnarMeshForSurfaceChunk(
				mesh,
				planarCellGrid,
				surfaceChunk,
				settings))
			{
				clearXZColumnarMeshes(meshes);
				return false;
			}

			if (mesh.vertices.empty() ||
				mesh.indices.empty())
			{
				continue;
			}

			meshes.push_back(
				std::move(mesh));
		}

		expectedSurfaceChunkIndex =
			columnEndIndex;
	}

	assert(
		expectedSurfaceChunkIndex ==
		surfaceMap.chunks.size());

	return true;
}
