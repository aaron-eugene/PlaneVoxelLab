///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.cpp
// ===============================================
//
// Implements CPU-side mesh construction for the XZ columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_builder.h"

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "experiments/xz_columnar/xz_columnar_patch.h"
#include "experiments/xz_columnar/xz_columnar_planar_cell.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/render_vertex.h"
#include "surface/surface_map.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

/***********************************************************
* Clipping Constants
************************************************************/

static constexpr float XZ_COLUMNAR_SIDE_EPSILON = 0.00001f;

/***********************************************************
* File-Local Types
************************************************************/

struct XZColumnarEdgeProfile
{
	glm::vec3 start = {};
	glm::vec3 end = {};
};

struct XZColumnarSideRegion
{
	XZColumnarEdgeProfile upper = {};
	XZColumnarEdgeProfile lower = {};

	int32_t ownerRelativeX = 0;
	int32_t ownerRelativeZ = 0;

	XZColumnarSide ownerSide =
		XZColumnarSide::PositiveX;
};

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

static glm::vec3 getColumnarSideNormal(
	XZColumnarSide side)
{
	switch (side)
	{
	case XZColumnarSide::NegativeX:
		return glm::vec3(-1.0f, 0.0f, 0.0f);

	case XZColumnarSide::PositiveX:
		return glm::vec3(1.0f, 0.0f, 0.0f);

	case XZColumnarSide::NegativeZ:
		return glm::vec3(0.0f, 0.0f, -1.0f);

	case XZColumnarSide::PositiveZ:
		return glm::vec3(0.0f, 0.0f, 1.0f);

	default:
		assert(false);
		return glm::vec3(0.0f, 1.0f, 0.0f);
	}
}

static glm::vec3 getSignedNormalColor(
	const glm::vec3& normal)
{
	const glm::vec3 normalizedNormal =
		glm::normalize(normal);

	return glm::vec3(0.5f) +
		normalizedNormal * 0.5f;
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

static XZColumnarClipPolygon getSideRegionPolygon(
	const XZColumnarSideRegion& region)
{
	XZColumnarClipPolygon polygon = {};

	switch (region.ownerSide)
	{
	case XZColumnarSide::NegativeX:
	case XZColumnarSide::PositiveZ:
	{
		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.start, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.start, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.end, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.end, {} });
	} break;

	case XZColumnarSide::PositiveX:
	case XZColumnarSide::NegativeZ:
	{
		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.start, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.end, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.end, {} });

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.start, {} });
	} break;

	default:
	{
		assert(false);
		return {};
	} break;
	}

	removeClosingDuplicateXZColumnarClipVertex(
		polygon);

	if (polygon.vertexCount < 3)
	{
		return {};
	}

	const glm::vec3 polygonNormal =
		getXZColumnarClipPolygonNormal(
			polygon);

	const glm::vec3 expectedNormal =
		getColumnarSideNormal(
			region.ownerSide);

	assert(
		glm::dot(
			polygonNormal,
			expectedNormal) > 0.0f);

	setXZColumnarClipPolygonColor(
		polygon,
		getSignedNormalColor(
			polygonNormal));

	return polygon;
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
		getSideRegionPolygon(
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
* Side Region Construction Helpers
************************************************************/

static XZColumnarEdgeProfile getNegativeXEdge(
	const XZColumnarPlanarCell& cell)
{
	return {
		cell.p00,
		cell.p01
	};
}

static XZColumnarEdgeProfile getPositiveXEdge(
	const XZColumnarPlanarCell& cell)
{
	return {
		cell.p10,
		cell.p11
	};
}

static XZColumnarEdgeProfile getNegativeZEdge(
	const XZColumnarPlanarCell& cell)
{
	return {
		cell.p00,
		cell.p10
	};
}

static XZColumnarEdgeProfile getPositiveZEdge(
	const XZColumnarPlanarCell& cell)
{
	return {
		cell.p01,
		cell.p11
	};
}

static glm::vec3 interpolateEdgeProfile(
	const XZColumnarEdgeProfile& profile,
	float t)
{
	assert(t >= 0.0f);
	assert(t <= 1.0f);

	return profile.start +
		(profile.end - profile.start) * t;
}

static XZColumnarSideRegion buildSideRegion(
	const XZColumnarPlanarCell& cellA,
	const XZColumnarEdgeProfile& profileA,
	XZColumnarSide sideA,
	const XZColumnarPlanarCell& cellB,
	const XZColumnarEdgeProfile& profileB,
	XZColumnarSide sideB,
	float startT,
	float endT)
{
	assert(startT >= 0.0f);
	assert(endT <= 1.0f);
	assert(endT > startT);

	const float sampleT =
		(startT + endT) * 0.5f;

	const glm::vec3 sampleA =
		interpolateEdgeProfile(
			profileA,
			sampleT);

	const glm::vec3 sampleB =
		interpolateEdgeProfile(
			profileB,
			sampleT);

	XZColumnarSideRegion region = {};

	if (sampleA.y > sampleB.y)
	{
		region.upper.start =
			interpolateEdgeProfile(
				profileA,
				startT);

		region.upper.end =
			interpolateEdgeProfile(
				profileA,
				endT);

		region.lower.start =
			interpolateEdgeProfile(
				profileB,
				startT);

		region.lower.end =
			interpolateEdgeProfile(
				profileB,
				endT);

		region.ownerRelativeX =
			cellA.relativeX;

		region.ownerRelativeZ =
			cellA.relativeZ;

		region.ownerSide =
			sideA;
	}
	else
	{
		region.upper.start =
			interpolateEdgeProfile(
				profileB,
				startT);

		region.upper.end =
			interpolateEdgeProfile(
				profileB,
				endT);

		region.lower.start =
			interpolateEdgeProfile(
				profileA,
				startT);

		region.lower.end =
			interpolateEdgeProfile(
				profileA,
				endT);

		region.ownerRelativeX =
			cellB.relativeX;

		region.ownerRelativeZ =
			cellB.relativeZ;

		region.ownerSide =
			sideB;
	}

	return region;
}

static uint32_t buildSharedEdgeSideRegions(
	XZColumnarSideRegion regions[2],
	const XZColumnarPlanarCell& cellA,
	const XZColumnarEdgeProfile& profileA,
	XZColumnarSide sideA,
	const XZColumnarPlanarCell& cellB,
	const XZColumnarEdgeProfile& profileB,
	XZColumnarSide sideB)
{
	const float startDifference =
		profileA.start.y -
		profileB.start.y;

	const float endDifference =
		profileA.end.y -
		profileB.end.y;

	const bool startEqual =
		std::abs(startDifference) <=
		XZ_COLUMNAR_SIDE_EPSILON;

	const bool endEqual =
		std::abs(endDifference) <=
		XZ_COLUMNAR_SIDE_EPSILON;

	if (startEqual &&
		endEqual)
	{
		return 0;
	}

	const bool crosses =
		!startEqual &&
		!endEqual &&
		((startDifference < 0.0f) !=
			(endDifference < 0.0f));

	if (!crosses)
	{
		regions[0] =
			buildSideRegion(
				cellA,
				profileA,
				sideA,
				cellB,
				profileB,
				sideB,
				0.0f,
				1.0f);

		return 1;
	}

	const float crossingT =
		startDifference /
		(startDifference - endDifference);

	assert(crossingT > 0.0f);
	assert(crossingT < 1.0f);

	regions[0] =
		buildSideRegion(
			cellA,
			profileA,
			sideA,
			cellB,
			profileB,
			sideB,
			0.0f,
			crossingT);

	regions[1] =
		buildSideRegion(
			cellA,
			profileA,
			sideA,
			cellB,
			profileB,
			sideB,
			crossingT,
			1.0f);

	return 2;
}



/***********************************************************
* Patch Construction Helpers
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
				buildSharedEdgeSideRegions(
					regions,
					negativeXCell,
					getPositiveXEdge(negativeXCell),
					XZColumnarSide::PositiveX,
					positiveXCell,
					getNegativeXEdge(positiveXCell),
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
				buildSharedEdgeSideRegions(
					regions,
					negativeZCell,
					getPositiveZEdge(negativeZCell),
					XZColumnarSide::PositiveZ,
					positiveZCell,
					getNegativeZEdge(positiveZCell),
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

		meshes.push_back(mesh);
	}

	return true;
}
