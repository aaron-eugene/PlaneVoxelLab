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
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/render_vertex.h"
#include "surface/surface_map.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

/***********************************************************
* Colorization Helpers
************************************************************/

static glm::vec3 getOwnerVoxelYColor(
	uint32_t localY)
{
	const float t =
		static_cast<float>(localY % 8) / 7.0f;

	return glm::vec3(
		0.35f + 0.45f * t,
		0.75f - 0.35f * t,
		0.55f + 0.25f * (1.0f - t));
}

static glm::vec3 getColumnarPieceColor(
	const XZColumnarClipPolygon& polygon,
	const XZColumnarBuildSettings& settings,
	const VoxelCoord& ownerVoxel)
{
	switch (settings.colorization)
	{
	case XZColumnarColorization::Normal:
	{
		assert(polygon.vertexCount > 0);
		return polygon.vertices[0].color;
	} break;

	case XZColumnarColorization::OwnerVoxelY:
	{
		return getOwnerVoxelYColor(ownerVoxel.y);
	} break;

	default:
	{
		assert(false);
		return glm::vec3(1.0f);
	} break;
	}
}

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
* Mesh Emission Helpers
************************************************************/

static uint32_t appendColoredVertexToMesh(
	XZColumnarMesh& mesh,
	const glm::vec3& worldPosition,
	const glm::vec3& chunkWorldMin,
	const glm::vec3& color)
{
	ColoredVertex vertex = {};
	vertex.position =
		worldPosition -
		chunkWorldMin;

	vertex.color =
		color;

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

static void appendVoxelOwnedPolygonToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel,
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

	const glm::vec3 pieceColor =
		getColumnarPieceColor(
			polygon,
			settings,
			ownerVoxel);

	const uint32_t baseVertexIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& clipVertex =
			polygon.vertices[vertexIndex];

		appendColoredVertexToMesh(
			mesh,
			clipVertex.position,
			chunkWorldMin,
			pieceColor);
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
		mesh.topPieces.push_back(topPiece);
	}
}

static void appendVoxelYSlicedPolygonToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	uint32_t localX,
	uint32_t localZ,
	const XZColumnarBuildSettings& settings)
{
	if (polygon.vertexCount < 3)
	{
		return;
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
		return;
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

		appendVoxelOwnedPolygonToMesh(
			mesh,
			voxelClippedPolygon,
			chunkWorldMin,
			ownerVoxel,
			settings);
	}
}

static void appendVoxelOwnedSidePolygonToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel,
	XZColumnarSide side,
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

	glm::vec3 fragmentColor =
		polygon.vertices[0].color;

	if (settings.colorization ==
		XZColumnarColorization::OwnerVoxelY)
	{
		fragmentColor =
			getOwnerVoxelYColor(
				ownerVoxel.y);
	}

	const uint32_t baseVertexIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		appendColoredVertexToMesh(
			mesh,
			polygon.vertices[
				vertexIndex].position,
				chunkWorldMin,
				fragmentColor);
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

static void appendOwnedSideRegionToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarSideRegion& region,
	const glm::vec3& chunkWorldMin,
	const XZColumnarBuildSettings& settings)
{
	if (!isXZColumnarPlanarCellCoordinateOwned(
		region.ownerRelativeX,
		region.ownerRelativeZ))
	{
		return;
	}

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

		appendVoxelOwnedSidePolygonToMesh(
			mesh,
			voxelClippedPolygon,
			chunkWorldMin,
			ownerVoxel,
			region.ownerSide,
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

			appendVoxelYSlicedPolygonToMesh(
				mesh,
				topPolygon,
				chunkWorldMin,
				localX,
				localZ,
				settings);
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
				appendOwnedSideRegionToMesh(
					mesh,
					regions[regionIndex],
					chunkWorldMin,
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
				appendOwnedSideRegionToMesh(
					mesh,
					regions[regionIndex],
					chunkWorldMin,
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

	meshes.reserve(surfaceMap.chunks.size());

	XZColumnarPlanarCellGrid planarCellGrid = {};

	initializeXZColumnarPlanarCellGrid(
		planarCellGrid);

	for (const SurfaceChunk& surfaceChunk : surfaceMap.chunks)
	{
		buildXZColumnarPlanarCellGrid(
			planarCellGrid,
			heightmap,
			surfaceChunk.coord,
			settings.derivativeStepMeters);
		
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

		meshes.push_back(std::move(mesh));
	}

	return true;
}
