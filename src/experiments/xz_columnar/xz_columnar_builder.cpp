///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.cpp
// ===============================================
//
// Implements CPU-side mesh construction for the XZ columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_builder.h"

#include "experiments/xz_columnar/xz_columnar_patch.h"
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

/***********************************************************
* Clipping Constants
************************************************************/

static constexpr float XZ_COLUMNAR_EPSILON = 0.00001f;
static constexpr uint32_t MAX_CLIPPED_POLYGON_VERTICES = 8;

/***********************************************************
* File-Local Types
************************************************************/

// Cached planar top for one XZ cell
struct XZColumnarPlanarCell
{
	int32_t relativeX = -1; // -1 to CHUNK_SIZE (for halo)
	int32_t relativeZ = -1; // -1 to CHUNK_SIZE (for halo)

	glm::vec3 p00 = {}; // min X, min Z
	glm::vec3 p01 = {}; // min X, max Z
	glm::vec3 p11 = {}; // max X, max Z
	glm::vec3 p10 = {}; // max X, min Z

	glm::vec3 color = {};
};

struct XZColumnarClipVertex
{
	glm::vec3 position = {};
	glm::vec3 color = {};
};

struct XZColumnarClipPolygon
{
	XZColumnarClipVertex vertices[MAX_CLIPPED_POLYGON_VERTICES] = {};
	uint32_t vertexCount = 0;
};

// One edge (line segment) of a planar top
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

static glm::vec3 getNormalColor(
	float gradientX,
	float gradientZ)
{
	const glm::vec3 normal =
		glm::normalize(
			glm::vec3(
				-gradientX,
				1.0f,
				-gradientZ));

	const glm::vec3 absoluteNormal =
		glm::abs(normal);

	return glm::vec3(
		0.55f + 0.25f * absoluteNormal.x,
		0.55f + 0.25f * absoluteNormal.y,
		0.55f + 0.25f * absoluteNormal.z);
}

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

static glm::vec3 getColumnarSideColor(
	const glm::vec3& topColor)
{
	return topColor * 0.75f;
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

static void setClipPolygonColor(
	XZColumnarClipPolygon& polygon,
	const glm::vec3& color)
{
	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		polygon.vertices[vertexIndex].color =
			color;
	}
}

/***********************************************************
* Y-Range Helpers
************************************************************/

static float getPolygonMinY(
	const XZColumnarClipPolygon& polygon)
{
	assert(polygon.vertexCount > 0);

	float minY =
		polygon.vertices[0].position.y;

	for (uint32_t vertexIndex = 1;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		minY =
			std::min(
				minY,
				polygon.vertices[vertexIndex].position.y);
	}

	return minY;
}

static float getPolygonMaxY(
	const XZColumnarClipPolygon& polygon)
{
	assert(polygon.vertexCount > 0);

	float maxY =
		polygon.vertices[0].position.y;

	for (uint32_t vertexIndex = 1;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		maxY =
			std::max(
				maxY,
				polygon.vertices[vertexIndex].position.y);
	}

	return maxY;
}

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
* Tangent Plane Helpers
************************************************************/

static float evaluateTangentPlaneHeight(
	float x,
	float z,
	float centerX,
	float centerZ,
	const XZColumnarPatchSample& sample)
{
	return sample.height +
		sample.gradientX * (x - centerX) +
		sample.gradientZ * (z - centerZ);
}

/***********************************************************
* Polygon Clipping Helpers
************************************************************/

static bool clipVertexPositionsNearlyEqual(
	const XZColumnarClipVertex& a,
	const XZColumnarClipVertex& b)
{
	return
		std::abs(a.position.x - b.position.x) <=
		XZ_COLUMNAR_EPSILON &&
		std::abs(a.position.y - b.position.y) <=
		XZ_COLUMNAR_EPSILON &&
		std::abs(a.position.z - b.position.z) <=
		XZ_COLUMNAR_EPSILON;
}

static void appendClipVertex(
	XZColumnarClipPolygon& polygon,
	const XZColumnarClipVertex& vertex)
{
	if (polygon.vertexCount > 0)
	{
		const XZColumnarClipVertex& previous =
			polygon.vertices[
				polygon.vertexCount - 1];

		if (clipVertexPositionsNearlyEqual(
			previous,
			vertex))
		{
			return;
		}
	}

	assert(
		polygon.vertexCount <
		MAX_CLIPPED_POLYGON_VERTICES);

	polygon.vertices[polygon.vertexCount] =
		vertex;

	++polygon.vertexCount;
}

static XZColumnarClipVertex interpolateClipVertexAtY(
	const XZColumnarClipVertex& a,
	const XZColumnarClipVertex& b,
	float planeY)
{
	const float deltaY =
		b.position.y - a.position.y;

	assert(std::abs(deltaY) > XZ_COLUMNAR_EPSILON);

	const float t =
		(planeY - a.position.y) / deltaY;

	XZColumnarClipVertex result = {};

	result.position =
		a.position +
		(b.position - a.position) * t;

	result.color =
		a.color +
		(b.color - a.color) * t;

	return result;
}

static void clipPolygonMinY(
	const XZColumnarClipPolygon& input,
	XZColumnarClipPolygon& output,
	float minY)
{
	output = {};

	if (input.vertexCount == 0)
	{
		return;
	}

	for (uint32_t vertexIndex = 0;
		vertexIndex < input.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& current =
			input.vertices[vertexIndex];

		const XZColumnarClipVertex& previous =
			input.vertices[
				(vertexIndex + input.vertexCount - 1) %
					input.vertexCount];

		const bool currentInside =
			current.position.y >= minY - XZ_COLUMNAR_EPSILON;

		const bool previousInside =
			previous.position.y >= minY - XZ_COLUMNAR_EPSILON;

		if (currentInside != previousInside)
		{
			appendClipVertex(
				output,
				interpolateClipVertexAtY(
					previous,
					current,
					minY));
		}

		if (currentInside)
		{
			appendClipVertex(
				output,
				current);
		}
	}
}

static void clipPolygonMaxY(
	const XZColumnarClipPolygon& input,
	XZColumnarClipPolygon& output,
	float maxY)
{
	output = {};

	if (input.vertexCount == 0)
	{
		return;
	}

	for (uint32_t vertexIndex = 0;
		vertexIndex < input.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& current =
			input.vertices[vertexIndex];

		const XZColumnarClipVertex& previous =
			input.vertices[
				(vertexIndex + input.vertexCount - 1) %
					input.vertexCount];

		const bool currentInside =
			current.position.y <=
			maxY + XZ_COLUMNAR_EPSILON;

		const bool previousInside =
			previous.position.y <=
			maxY + XZ_COLUMNAR_EPSILON;

		if (currentInside != previousInside)
		{
			appendClipVertex(
				output,
				interpolateClipVertexAtY(
					previous,
					current,
					maxY));
		}

		if (currentInside)
		{
			appendClipVertex(
				output,
				current);
		}
	}
}

static XZColumnarClipPolygon clipPolygonToYSlab(
	const XZColumnarClipPolygon& polygon,
	float minY,
	float maxY)
{
	assert(maxY > minY);

	XZColumnarClipPolygon clippedMin = {};
	XZColumnarClipPolygon clippedMax = {};

	clipPolygonMinY(
		polygon,
		clippedMin,
		minY);

	clipPolygonMaxY(
		clippedMin,
		clippedMax,
		maxY);

	return clippedMax;
}

static XZColumnarClipPolygon getPlanarCellTopPolygon(
	const XZColumnarPlanarCell& cell)
{
	XZColumnarClipPolygon polygon = {};

	appendClipVertex(
		polygon,
		{ cell.p00, cell.color });

	appendClipVertex(
		polygon,
		{ cell.p01, cell.color });

	appendClipVertex(
		polygon,
		{ cell.p11, cell.color });

	appendClipVertex(
		polygon,
		{ cell.p10, cell.color });

	return polygon;
}

static void removeClosingDuplicateClipVertex(
	XZColumnarClipPolygon& polygon)
{
	if (polygon.vertexCount < 2)
	{
		return;
	}

	if (clipVertexPositionsNearlyEqual(
		polygon.vertices[0],
		polygon.vertices[
			polygon.vertexCount - 1]))
	{
		--polygon.vertexCount;
	}
}

static glm::vec3 getClipPolygonNormal(
	const XZColumnarClipPolygon& polygon)
{
	assert(polygon.vertexCount >= 3);

	for (uint32_t vertexIndex = 1;
		vertexIndex + 1 < polygon.vertexCount;
		++vertexIndex)
	{
		const glm::vec3 edgeA =
			polygon.vertices[vertexIndex].position -
			polygon.vertices[0].position;

		const glm::vec3 edgeB =
			polygon.vertices[vertexIndex + 1].position -
			polygon.vertices[0].position;

		const glm::vec3 crossProduct =
			glm::cross(
				edgeA,
				edgeB);

		const float lengthSquared =
			glm::dot(
				crossProduct,
				crossProduct);

		if (lengthSquared >
			XZ_COLUMNAR_EPSILON *
			XZ_COLUMNAR_EPSILON)
		{
			return glm::normalize(
				crossProduct);
		}
	}

	assert(false);
	return glm::vec3(0.0f, 1.0f, 0.0f);
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
		clipPolygonToYSlab(
			polygon,
			chunkMinY,
			chunkMaxY);

	if (chunkClippedPolygon.vertexCount < 3)
	{
		return;
	}

	const float polygonMinY =
		getPolygonMinY(chunkClippedPolygon);

	const float polygonMaxY =
		getPolygonMaxY(chunkClippedPolygon);

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
			clipPolygonToYSlab(
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

/***********************************************************
* Halo-Grid Helpers
************************************************************/

static constexpr uint32_t XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE =
CHUNK_SIZE + 2;

static uint32_t getPlanarCellGridIndex(
	int32_t relativeX,
	int32_t relativeZ)
{
	assert(relativeX >= -1);
	assert(
		relativeX <=
		static_cast<int32_t>(CHUNK_SIZE));

	assert(relativeZ >= -1);
	assert(
		relativeZ <=
		static_cast<int32_t>(CHUNK_SIZE));

	const uint32_t gridX =
		static_cast<uint32_t>(
			relativeX + 1);

	const uint32_t gridZ =
		static_cast<uint32_t>(
			relativeZ + 1);

	return gridX +
		gridZ *
		XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE;
}

static const XZColumnarPlanarCell& getPlanarCell(
	const std::vector<XZColumnarPlanarCell>& planarCells,
	int32_t relativeX,
	int32_t relativeZ)
{
	const uint32_t index =
		getPlanarCellGridIndex(
			relativeX,
			relativeZ);

	assert(index < planarCells.size());

	return planarCells[index];
}

static bool isCellCoordinateOwnedByCurrentChunk(
	int32_t relativeX,
	int32_t relativeZ)
{
	return
		relativeX >= 0 &&
		relativeX <
		static_cast<int32_t>(CHUNK_SIZE) &&
		relativeZ >= 0 &&
		relativeZ <
		static_cast<int32_t>(CHUNK_SIZE);
}

/***********************************************************
* Planar Cell Edge Helpers
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
		XZ_COLUMNAR_EPSILON;

	const bool endEqual =
		std::abs(endDifference) <=
		XZ_COLUMNAR_EPSILON;

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

static XZColumnarClipPolygon getSideRegionPolygon(
	const XZColumnarSideRegion& region)
{
	XZColumnarClipPolygon polygon = {};

	switch (region.ownerSide)
	{
	case XZColumnarSide::NegativeX:
	case XZColumnarSide::PositiveZ:
	{
		appendClipVertex(
			polygon,
			{ region.upper.start, {} });

		appendClipVertex(
			polygon,
			{ region.lower.start, {} });

		appendClipVertex(
			polygon,
			{ region.lower.end, {} });

		appendClipVertex(
			polygon,
			{ region.upper.end, {} });
	} break;

	case XZColumnarSide::PositiveX:
	case XZColumnarSide::NegativeZ:
	{
		appendClipVertex(
			polygon,
			{ region.upper.start, {} });

		appendClipVertex(
			polygon,
			{ region.upper.end, {} });

		appendClipVertex(
			polygon,
			{ region.lower.end, {} });

		appendClipVertex(
			polygon,
			{ region.lower.start, {} });
	} break;

	default:
	{
		assert(false);
		return {};
	} break;
	}

	removeClosingDuplicateClipVertex(
		polygon);

	if (polygon.vertexCount < 3)
	{
		return {};
	}

	const glm::vec3 polygonNormal =
		getClipPolygonNormal(
			polygon);

	const glm::vec3 expectedNormal =
		getColumnarSideNormal(
			region.ownerSide);

	assert(
		glm::dot(
			polygonNormal,
			expectedNormal) > 0.0f);

	setClipPolygonColor(
		polygon,
		getSignedNormalColor(
			polygonNormal));

	return polygon;
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
	if (!isCellCoordinateOwnedByCurrentChunk(
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
		clipPolygonToYSlab(
			sidePolygon,
			chunkMinY,
			chunkMaxY);

	if (chunkClippedPolygon.vertexCount < 3)
	{
		return;
	}

	const uint32_t firstLocalY =
		getClampedLocalVoxelYFromWorldY(
			getPolygonMinY(
				chunkClippedPolygon),
			chunkMinY);

	const uint32_t lastLocalY =
		getClampedLocalVoxelYFromWorldY(
			getPolygonMaxY(
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
			clipPolygonToYSlab(
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
* Patch Construction Helpers
************************************************************/

static XZColumnarPlanarCell buildXZColumnarPlanarCell(
	const HeightmapDensityField& heightmap,
	int32_t relativeX,
	int32_t relativeZ,
	float x0,
	float x1,
	float z0,
	float z1,
	const XZColumnarBuildSettings& settings)
{
	const XZColumnarPatchSample patchSample =
		sampleXZColumnarPatchCenter(
			heightmap,
			x0,
			x1,
			z0,
			z1,
			settings.derivativeStepMeters);

	const float centerX =
		(x0 + x1) * 0.5f;

	const float centerZ =
		(z0 + z1) * 0.5f;

	XZColumnarPlanarCell cell = {};
	cell.relativeX = relativeX;
	cell.relativeZ = relativeZ;

	cell.color =
		getNormalColor(
			patchSample.gradientX,
			patchSample.gradientZ);

	cell.p00 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z0,
				centerX,
				centerZ,
				patchSample),
			z0);

	cell.p01 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z1,
				centerX,
				centerZ,
				patchSample),
			z1);

	cell.p11 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z1,
				centerX,
				centerZ,
				patchSample),
			z1);

	cell.p10 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z0,
				centerX,
				centerZ,
				patchSample),
			z0);

	return cell;
}

static bool buildXZColumnarMeshForSurfaceChunk(
	XZColumnarMesh& mesh,
	std::vector<XZColumnarPlanarCell>& planarCells,
	const HeightmapDensityField& heightmap,
	const SurfaceChunk& surfaceChunk,
	const XZColumnarBuildSettings& settings)
{
	assert(
		planarCells.size() ==
		XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE *
		XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE);

	mesh = {};
	mesh.coord = surfaceChunk.coord;

	const glm::dvec3 chunkWorldMinD =
		getChunkWorldMin(
			surfaceChunk.coord);

	const glm::vec3 chunkWorldMin =
		glm::vec3(chunkWorldMinD);

	//--------------------------------------------------
	// Build Planar Cell Grid
	//--------------------------------------------------
	for (int32_t relativeZ = -1;
		relativeZ <= static_cast<int32_t>(CHUNK_SIZE);
		++relativeZ)
	{
		for (int32_t relativeX = -1;
			relativeX <= static_cast<int32_t>(CHUNK_SIZE);
			++relativeX)
		{
			const float x0 =
				chunkWorldMin.x +
				static_cast<float>(relativeX) *
				VOXEL_SIZE_METERS;

			const float x1 =
				x0 + VOXEL_SIZE_METERS;

			const float z0 =
				chunkWorldMin.z +
				static_cast<float>(relativeZ) *
				VOXEL_SIZE_METERS;

			const float z1 =
				z0 + VOXEL_SIZE_METERS;

			const uint32_t cellIndex =
				getPlanarCellGridIndex(
					relativeX,
					relativeZ);

			planarCells[cellIndex] =
				buildXZColumnarPlanarCell(
					heightmap,
					relativeX,
					relativeZ,
					x0,
					x1,
					z0,
					z1,
					settings);
		}
	}

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
				getPlanarCell(
					planarCells,
					static_cast<int32_t>(localX),
					static_cast<int32_t>(localZ));

			const XZColumnarClipPolygon topPolygon =
				getPlanarCellTopPolygon(
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
				getPlanarCell(
					planarCells,
					negativeX,
					relativeZ);

			const XZColumnarPlanarCell& positiveXCell =
				getPlanarCell(
					planarCells,
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
				getPlanarCell(
					planarCells,
					relativeX,
					negativeZ);

			const XZColumnarPlanarCell& positiveZCell =
				getPlanarCell(
					planarCells,
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

/***********************************************************
* Columnar Patch Mesh Building
************************************************************/

bool buildXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes,
	const HeightmapDensityField& heightmap,
	const SurfaceMap& surfaceMap,
	const XZColumnarBuildSettings& settings)
{
	assert(settings.derivativeStepMeters > 0.0f);

	clearXZColumnarMeshes(meshes);

	meshes.reserve(surfaceMap.chunks.size());

	const size_t planarCellCount =
		static_cast<size_t>(
			XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE) *
			XZ_COLUMNAR_PLANAR_CELL_GRID_SIZE;

	std::vector<XZColumnarPlanarCell> planarCells(
		planarCellCount);

	for (const SurfaceChunk& surfaceChunk : surfaceMap.chunks)
	{
		XZColumnarMesh mesh = {};

		if (!buildXZColumnarMeshForSurfaceChunk(
			mesh,
			planarCells,
			heightmap,
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
