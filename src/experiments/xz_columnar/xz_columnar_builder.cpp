///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.cpp
// ===============================================
//
// Implements CPU-side mesh construction for the XZ columnar experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_builder.h"

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
* Constants
************************************************************/

static constexpr float XZ_COLUMNAR_EPSILON = 0.00001f;
static constexpr uint32_t MAX_CLIPPED_POLYGON_VERTICES = 8;

/***********************************************************
* File-Local Types
************************************************************/

struct XZColumnarPatchCorner
{
	float x = 0.0f;
	float z = 0.0f;

	float height = 0.0f;

	float gradientX = 0.0f;
	float gradientZ = 0.0f;
	float gradientXZ = 0.0f;
};

struct XZColumnarPatchCenter
{
	float height = 0.0f;

	float gradientX = 0.0f;
	float gradientZ = 0.0f;
};

struct XZColumnarClipPolygon
{
	ColoredVertex vertices[MAX_CLIPPED_POLYGON_VERTICES] = {};
	uint32_t vertexCount = 0;
};

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
* Height Sampling Helpers
************************************************************/

static float sampleHeight(
	const HeightmapDensityField& heightmap,
	float x,
	float z)
{
	return sampleHeightmapTerrainHeight(
		heightmap,
		x,
		z);
}

static XZColumnarPatchCorner samplePatchCorner(
	const HeightmapDensityField& heightmap,
	float x,
	float z,
	float derivativeStepMeters)
{
	assert(derivativeStepMeters > 0.0f);

	XZColumnarPatchCorner corner = {};
	corner.x = x;
	corner.z = z;

	corner.height =
		sampleHeight(
			heightmap,
			x,
			z);

	const float step =
		derivativeStepMeters;

	const float heightX0 =
		sampleHeight(
			heightmap,
			x - step,
			z);

	const float heightX1 =
		sampleHeight(
			heightmap,
			x + step,
			z);

	const float heightZ0 =
		sampleHeight(
			heightmap,
			x,
			z - step);

	const float heightZ1 =
		sampleHeight(
			heightmap,
			x,
			z + step);

	corner.gradientX =
		(heightX1 - heightX0) /
		(2.0f * step);

	corner.gradientZ =
		(heightZ1 - heightZ0) /
		(2.0f * step);

	const float heightX0Z0 =
		sampleHeight(
			heightmap,
			x - step,
			z - step);

	const float heightX0Z1 =
		sampleHeight(
			heightmap,
			x - step,
			z + step);

	const float heightX1Z0 =
		sampleHeight(
			heightmap,
			x + step,
			z - step);

	const float heightX1Z1 =
		sampleHeight(
			heightmap,
			x + step,
			z + step);

	corner.gradientXZ =
		(heightX1Z1 -
			heightX1Z0 -
			heightX0Z1 +
			heightX0Z0) /
		(4.0f * step * step);

	return corner;
}

/***********************************************************
* Bicubic Patch Helpers
************************************************************/

static void evaluateHermiteBasis(
	float t,
	float basis[4],
	float derivativeBasis[4])
{
	const float t2 = t * t;
	const float t3 = t2 * t;

	basis[0] = 2.0f * t3 - 3.0f * t2 + 1.0f;
	basis[1] = -2.0f * t3 + 3.0f * t2;
	basis[2] = t3 - 2.0f * t2 + t;
	basis[3] = t3 - t2;

	derivativeBasis[0] = 6.0f * t2 - 6.0f * t;
	derivativeBasis[1] = -6.0f * t2 + 6.0f * t;
	derivativeBasis[2] = 3.0f * t2 - 4.0f * t + 1.0f;
	derivativeBasis[3] = 3.0f * t2 - 2.0f * t;
}

static XZColumnarPatchCenter evaluateBicubicPatchCenter(
	const XZColumnarPatchCorner& corner00,
	const XZColumnarPatchCorner& corner10,
	const XZColumnarPatchCorner& corner01,
	const XZColumnarPatchCorner& corner11)
{
	const float cellSizeX =
		corner10.x - corner00.x;

	const float cellSizeZ =
		corner01.z - corner00.z;

	assert(cellSizeX > 0.0f);
	assert(cellSizeZ > 0.0f);

	float basisU[4] = {};
	float derivativeBasisU[4] = {};
	float basisV[4] = {};
	float derivativeBasisV[4] = {};

	evaluateHermiteBasis(
		0.5f,
		basisU,
		derivativeBasisU);

	evaluateHermiteBasis(
		0.5f,
		basisV,
		derivativeBasisV);

	// Bicubic Hermite data matrix:
	// [ h00, h01, hz00, hz01 ]
	// [ h10, h11, hz10, hz11 ]
	// [ hx00, hx01, hxz00, hxz01 ]
	// [ hx10, hx11, hxz10, hxz11 ]
	float patchData[4][4] = {};

	patchData[0][0] = corner00.height;
	patchData[1][0] = corner10.height;
	patchData[0][1] = corner01.height;
	patchData[1][1] = corner11.height;

	patchData[2][0] = corner00.gradientX * cellSizeX;
	patchData[3][0] = corner10.gradientX * cellSizeX;
	patchData[2][1] = corner01.gradientX * cellSizeX;
	patchData[3][1] = corner11.gradientX * cellSizeX;

	patchData[0][2] = corner00.gradientZ * cellSizeZ;
	patchData[1][2] = corner10.gradientZ * cellSizeZ;
	patchData[0][3] = corner01.gradientZ * cellSizeZ;
	patchData[1][3] = corner11.gradientZ * cellSizeZ;

	patchData[2][2] =
		corner00.gradientXZ * cellSizeX * cellSizeZ;

	patchData[3][2] =
		corner10.gradientXZ * cellSizeX * cellSizeZ;

	patchData[2][3] =
		corner01.gradientXZ * cellSizeX * cellSizeZ;

	patchData[3][3] =
		corner11.gradientXZ * cellSizeX * cellSizeZ;

	float height = 0.0f;
	float derivativeU = 0.0f;
	float derivativeV = 0.0f;

	for (uint32_t uIndex = 0;
		uIndex < 4;
		++uIndex)
	{
		for (uint32_t vIndex = 0;
			vIndex < 4;
			++vIndex)
		{
			const float value =
				patchData[uIndex][vIndex];

			height +=
				basisU[uIndex] *
				basisV[vIndex] *
				value;

			derivativeU +=
				derivativeBasisU[uIndex] *
				basisV[vIndex] *
				value;

			derivativeV +=
				basisU[uIndex] *
				derivativeBasisV[vIndex] *
				value;
		}
	}

	XZColumnarPatchCenter center = {};
	center.height = height;
	center.gradientX = derivativeU / cellSizeX;
	center.gradientZ = derivativeV / cellSizeZ;

	return center;
}

static float evaluateTangentPlaneHeight(
	float x,
	float z,
	float centerX,
	float centerZ,
	const XZColumnarPatchCenter& center)
{
	return center.height +
		center.gradientX * (x - centerX) +
		center.gradientZ * (z - centerZ);
}

/***********************************************************
* Polygon Clipping Helpers
************************************************************/

static glm::vec3 getColumnarColor(
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

static void appendClipVertex(
	XZColumnarClipPolygon& polygon,
	const ColoredVertex& vertex)
{
	assert(polygon.vertexCount < MAX_CLIPPED_POLYGON_VERTICES);

	polygon.vertices[polygon.vertexCount] = vertex;
	++polygon.vertexCount;
}

static ColoredVertex interpolateClipVertexAtY(
	const ColoredVertex& a,
	const ColoredVertex& b,
	float planeY)
{
	const float deltaY =
		b.position.y - a.position.y;

	assert(std::abs(deltaY) > XZ_COLUMNAR_EPSILON);

	const float t =
		(planeY - a.position.y) / deltaY;

	ColoredVertex result = {};
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
		const ColoredVertex& current =
			input.vertices[vertexIndex];

		const ColoredVertex& previous =
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
		const ColoredVertex& current =
			input.vertices[vertexIndex];

		const ColoredVertex& previous =
			input.vertices[
				(vertexIndex + input.vertexCount - 1) %
					input.vertexCount];

		const bool currentInside =
			current.position.y <= maxY + XZ_COLUMNAR_EPSILON;

		const bool previousInside =
			previous.position.y <= maxY + XZ_COLUMNAR_EPSILON;

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

/***********************************************************
* Mesh Emission Helpers
************************************************************/

static void appendVoxelOwnedPolygonToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	const VoxelCoord& ownerVoxel)
{
	if (polygon.vertexCount < 3)
	{
		return;
	}

	XZColumnarPiece piece = {};
	piece.ownerVoxel = ownerVoxel;
	piece.firstIndex =
		static_cast<uint32_t>(mesh.indices.size());

	const uint32_t baseVertexIndex =
		static_cast<uint32_t>(mesh.vertices.size());

	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		ColoredVertex localVertex =
			polygon.vertices[vertexIndex];

		localVertex.position -= chunkWorldMin;

		mesh.vertices.push_back(localVertex);
	}

	for (uint32_t vertexIndex = 1;
		vertexIndex + 1 < polygon.vertexCount;
		++vertexIndex)
	{
		mesh.indices.push_back(baseVertexIndex);
		mesh.indices.push_back(baseVertexIndex + vertexIndex);
		mesh.indices.push_back(baseVertexIndex + vertexIndex + 1);
	}

	piece.indexCount =
		static_cast<uint32_t>(mesh.indices.size()) -
		piece.firstIndex;

	if (piece.indexCount > 0)
	{
		mesh.pieces.push_back(piece);
	}
}

static void appendVoxelYSlicedPolygonToMesh(
	XZColumnarMesh& mesh,
	const XZColumnarClipPolygon& polygon,
	const glm::vec3& chunkWorldMin,
	uint32_t localX,
	uint32_t localZ)
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
			ownerVoxel);
	}
}



/***********************************************************
* Patch Construction Helpers
************************************************************/

static XZColumnarClipPolygon buildPlanarColumnarQuad(
	const HeightmapDensityField& heightmap,
	float x0,
	float x1,
	float z0,
	float z1,
	const XZColumnarBuildSettings& settings)
{
	const XZColumnarPatchCorner corner00 =
		samplePatchCorner(
			heightmap,
			x0,
			z0,
			settings.derivativeStepMeters);

	const XZColumnarPatchCorner corner10 =
		samplePatchCorner(
			heightmap,
			x1,
			z0,
			settings.derivativeStepMeters);

	const XZColumnarPatchCorner corner01 =
		samplePatchCorner(
			heightmap,
			x0,
			z1,
			settings.derivativeStepMeters);

	const XZColumnarPatchCorner corner11 =
		samplePatchCorner(
			heightmap,
			x1,
			z1,
			settings.derivativeStepMeters);

	const XZColumnarPatchCenter center =
		evaluateBicubicPatchCenter(
			corner00,
			corner10,
			corner01,
			corner11);

	const float centerX =
		(x0 + x1) * 0.5f;

	const float centerZ =
		(z0 + z1) * 0.5f;

	const glm::vec3 color =
		getColumnarColor(
			center.gradientX,
			center.gradientZ);

	const glm::vec3 p00 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z0,
				centerX,
				centerZ,
				center),
			z0);

	const glm::vec3 p01 =
		glm::vec3(
			x0,
			evaluateTangentPlaneHeight(
				x0,
				z1,
				centerX,
				centerZ,
				center),
			z1);

	const glm::vec3 p11 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z1,
				centerX,
				centerZ,
				center),
			z1);

	const glm::vec3 p10 =
		glm::vec3(
			x1,
			evaluateTangentPlaneHeight(
				x1,
				z0,
				centerX,
				centerZ,
				center),
			z0);

	XZColumnarClipPolygon polygon = {};

	appendClipVertex(
		polygon,
		{ p00, color });

	appendClipVertex(
		polygon,
		{ p01, color });

	appendClipVertex(
		polygon,
		{ p11, color });

	appendClipVertex(
		polygon,
		{ p10, color });

	return polygon;
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

			const XZColumnarClipPolygon planarQuad =
				buildPlanarColumnarQuad(
					heightmap,
					x0,
					x1,
					z0,
					z1,
					settings);

			appendVoxelYSlicedPolygonToMesh(
				mesh,
				planarQuad,
				chunkWorldMin,
				localX,
				localZ);
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
