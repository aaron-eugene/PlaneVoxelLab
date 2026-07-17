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

struct XZColumnarPlanarCell
{
	uint32_t localX = 0;
	uint32_t localZ = 0;

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

static void appendClipVertex(
	XZColumnarClipPolygon& polygon,
	const XZColumnarClipVertex& vertex)
{
	assert(polygon.vertexCount < MAX_CLIPPED_POLYGON_VERTICES);

	polygon.vertices[polygon.vertexCount] = vertex;
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
* Patch Construction Helpers
************************************************************/

static XZColumnarPlanarCell buildXZColumnarPlanarCell(
	const HeightmapDensityField& heightmap,
	uint32_t localX,
	uint32_t localZ,
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
	cell.localX = localX;
	cell.localZ = localZ;

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
	const HeightmapDensityField& heightmap,
	const SurfaceChunk& surfaceChunk,
	const XZColumnarBuildSettings& settings)
{
	mesh = {};
	mesh.coord = surfaceChunk.coord;

	const glm::dvec3 chunkWorldMinD =
		getChunkWorldMin(surfaceChunk.coord);

	const glm::vec3 chunkWorldMin =
		glm::vec3(chunkWorldMinD);

	for (uint32_t localZ = 0;
		localZ < CHUNK_SIZE;
		++localZ)
	{
		for (uint32_t localX = 0;
			localX < CHUNK_SIZE;
			++localX)
		{
			const float x0 =
				chunkWorldMin.x +
				static_cast<float>(localX) *
				VOXEL_SIZE_METERS;

			const float x1 =
				x0 + VOXEL_SIZE_METERS;

			const float z0 =
				chunkWorldMin.z +
				static_cast<float>(localZ) *
				VOXEL_SIZE_METERS;

			const float z1 =
				z0 + VOXEL_SIZE_METERS;

			const XZColumnarPlanarCell planarCell =
				buildXZColumnarPlanarCell(
					heightmap,
					localX,
					localZ,
					x0,
					x1,
					z0,
					z1,
					settings);

			const XZColumnarClipPolygon topPolygon =
				getPlanarCellTopPolygon(
					planarCell);

			appendVoxelYSlicedPolygonToMesh(
				mesh,
				topPolygon,
				chunkWorldMin,
				planarCell.localX,
				planarCell.localZ,
				settings);
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

	for (const SurfaceChunk& surfaceChunk : surfaceMap.chunks)
	{
		XZColumnarMesh mesh = {};

		if (!buildXZColumnarMeshForSurfaceChunk(
			mesh,
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
