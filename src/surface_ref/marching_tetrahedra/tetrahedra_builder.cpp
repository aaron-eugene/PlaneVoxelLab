///////////////////////////////////////////////////////////////////////////////
// surface_ref/marching_tetrahedra/tetrahedra_builder.cpp
// ======================================================
//
///////////////////////////////////////////////////////////////////////////////

#include "surface_ref/marching_tetrahedra/tetrahedra_builder.h"

#include "chunk/chunk.h"
#include "geometry/voxel_topology.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"
#include "surface/surface_map.h"
#include "surface_ref/marching_tetrahedra/tetrahedra_tables.h"

#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

#include <cassert>
#include <cstdint>

/***********************************************************
* File-Local Constants
************************************************************/

static constexpr float SURFACE_DENSITY = 0.0f;

static constexpr float SURFACE_REFERENCE_MIN_BRIGHTNESS = 0.25f;
static constexpr float SURFACE_REFERENCE_MAX_BRIGHTNESS = 0.90f;

/***********************************************************
* Local Types
************************************************************/

struct TetrahedronCorner
{
	glm::vec3 position = {};
	float density = 0.0f;
};

struct TetrahedronSurfacePolygon
{
	glm::vec3 points[4] = {};
	uint32_t pointCount = 0;
};

/***********************************************************
* Coordinate Helpers
************************************************************/

static SampleCoord getVoxelCornerSampleCoord(
	const VoxelCoord& voxelCoord,
	uint32_t cornerIndex)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));
	assert(cornerIndex < VOXEL_CORNER_COUNT);

	const VoxelCornerGridOffset cornerOffset =
		getVoxelCornerGridOffset(cornerIndex);

	SampleCoord sampleCoord = {};
	sampleCoord.x = voxelCoord.x + cornerOffset.x;
	sampleCoord.y = voxelCoord.y + cornerOffset.y;
	sampleCoord.z = voxelCoord.z + cornerOffset.z;

	assert(isSampleCoordInChunkBounds(sampleCoord));

	return sampleCoord;
}

static glm::vec3 getVoxelCornerLocalPosition(
	const VoxelCoord& voxelCoord,
	uint32_t cornerIndex)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));
	assert(cornerIndex < VOXEL_CORNER_COUNT);

	return
		getVoxelLocalMin(voxelCoord) +
		getVoxelCornerMetricOffset(cornerIndex);
}

/***********************************************************
* Density Helpers
************************************************************/

static bool isDensityInside(
	float density)
{
	return density < SURFACE_DENSITY;
}

/***********************************************************
* Mesh Helpers
************************************************************/

static glm::vec3 calculateTriangleNormal(
	const glm::vec3& pointA,
	const glm::vec3& pointB,
	const glm::vec3& pointC)
{
	const glm::vec3 edgeAB = pointB - pointA;
	const glm::vec3 edgeAC = pointC - pointA;

	const glm::vec3 normal =
		glm::cross(edgeAB, edgeAC);

	const float normalLength =
		glm::length(normal);

	if (normalLength == 0.0f)
	{
		return glm::vec3(0.0f, 1.0f, 0.0f);
	}

	return normal / normalLength;
}

static glm::vec3 getSurfaceReferenceNormalColor(
	const glm::vec3& normal)
{
	const glm::vec3 absoluteNormal =
		glm::abs(normal);

	const glm::vec3 mutedNormalColor =
		glm::vec3(
			0.55f + 0.25f * absoluteNormal.x,
			0.55f + 0.25f * absoluteNormal.y,
			0.55f + 0.25f * absoluteNormal.z);

	return mutedNormalColor;
}

static glm::vec3 interpolateSurfacePoint(
	const glm::vec3& positionA,
	float densityA,
	const glm::vec3& positionB,
	float densityB)
{
	const float denominator = densityA - densityB;

	if (denominator == 0.0f)
	{
		return positionA;
	}

	const float t =
		(densityA - SURFACE_DENSITY) / denominator;

	return positionA + t * (positionB - positionA);
}

static void addOrientedTriangle(
	TetrahedraMesh& mesh,
	const glm::vec3& pointA,
	const glm::vec3& pointB,
	const glm::vec3& pointC,
	const glm::vec3& desiredNormalDirection)
{
	glm::vec3 finalPointB = pointB;
	glm::vec3 finalPointC = pointC;

	const glm::vec3 normal =
		calculateTriangleNormal(
			pointA,
			pointB,
			pointC);

	if (glm::dot(normal, desiredNormalDirection) < 0.0f)
	{
		finalPointB = pointC;
		finalPointC = pointB;
	}

	const uint32_t baseIndex =
		static_cast<uint32_t>(mesh.vertices.size());

	const glm::vec3 finalNormal =
		calculateTriangleNormal(
			pointA,
			finalPointB,
			finalPointC);

	const glm::vec3 color =
		getSurfaceReferenceNormalColor(finalNormal);

	mesh.vertices.push_back({ pointA, color });
	mesh.vertices.push_back({ finalPointB, color });
	mesh.vertices.push_back({ finalPointC, color });

	mesh.indices.push_back(baseIndex + 0);
	mesh.indices.push_back(baseIndex + 1);
	mesh.indices.push_back(baseIndex + 2);
}

/***********************************************************
* Tetrahedra Helpers
************************************************************/

static uint32_t gatherInsideOutsideCornerIndices(
	uint32_t insideCorners[MARCHING_TETRAHEDRON_CORNER_COUNT],
	uint32_t& insideCount,
	uint32_t outsideCorners[MARCHING_TETRAHEDRON_CORNER_COUNT],
	uint32_t& outsideCount,
	const TetrahedronCorner tetrahedronCorners[MARCHING_TETRAHEDRON_CORNER_COUNT])
{
	insideCount = 0;
	outsideCount = 0;

	for (uint32_t cornerIndex = 0;
		cornerIndex < MARCHING_TETRAHEDRON_CORNER_COUNT;
		++cornerIndex)
	{
		if (isDensityInside(tetrahedronCorners[cornerIndex].density))
		{
			insideCorners[insideCount] = cornerIndex;
			++insideCount;
		}
		else
		{
			outsideCorners[outsideCount] = cornerIndex;
			++outsideCount;
		}
	}

	assert(insideCount + outsideCount == MARCHING_TETRAHEDRON_CORNER_COUNT);

	return insideCount;
}

static glm::vec3 getCornerGroupCentroid(
	const TetrahedronCorner tetrahedronCorners[MARCHING_TETRAHEDRON_CORNER_COUNT],
	const uint32_t* cornerIndices,
	uint32_t cornerCount)
{
	assert(cornerCount > 0);

	glm::vec3 centroid = {};

	for (uint32_t index = 0;
		index < cornerCount;
		++index)
	{
		const uint32_t cornerIndex = cornerIndices[index];

		assert(cornerIndex < MARCHING_TETRAHEDRON_CORNER_COUNT);

		centroid += tetrahedronCorners[cornerIndex].position;
	}

	return centroid / static_cast<float>(cornerCount);
}

static glm::vec3 getSurfaceDirectionFromSolidToAir(
	const TetrahedronCorner tetrahedronCorners[MARCHING_TETRAHEDRON_CORNER_COUNT],
	const uint32_t* insideCorners,
	uint32_t insideCount,
	const uint32_t* outsideCorners,
	uint32_t outsideCount)
{
	assert(insideCount > 0);
	assert(outsideCount > 0);

	const glm::vec3 insideCentroid =
		getCornerGroupCentroid(
			tetrahedronCorners,
			insideCorners,
			insideCount);

	const glm::vec3 outsideCentroid =
		getCornerGroupCentroid(
			tetrahedronCorners,
			outsideCorners,
			outsideCount);

	return outsideCentroid - insideCentroid;
}

static glm::vec3 getTetrahedronEdgeSurfacePoint(
	const TetrahedronCorner& cornerA,
	const TetrahedronCorner& cornerB)
{
	return interpolateSurfacePoint(
		cornerA.position,
		cornerA.density,
		cornerB.position,
		cornerB.density);
}

static void gatherTetrahedronCorners(
	TetrahedronCorner tetrahedronCorners[MARCHING_TETRAHEDRON_CORNER_COUNT],
	const glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT],
	const float voxelCornerDensities[VOXEL_CORNER_COUNT],
	uint32_t tetrahedronIndex)
{
	assert(tetrahedronIndex < MARCHING_TETRAHEDRA_PER_VOXEL);

	for (uint32_t tetrahedronCornerIndex = 0;
		tetrahedronCornerIndex < MARCHING_TETRAHEDRON_CORNER_COUNT;
		++tetrahedronCornerIndex)
	{
		const uint32_t voxelCornerIndex =
			MARCHING_TETRAHEDRA_CORNERS
			[tetrahedronIndex]
			[tetrahedronCornerIndex];

		assert(voxelCornerIndex < VOXEL_CORNER_COUNT);

		tetrahedronCorners[tetrahedronCornerIndex].position =
			voxelCornerPositions[voxelCornerIndex];

		tetrahedronCorners[tetrahedronCornerIndex].density =
			voxelCornerDensities[voxelCornerIndex];
	}
}

static void polygonizeTetrahedron(
	TetrahedraMesh& mesh,
	const glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT],
	const float voxelCornerDensities[VOXEL_CORNER_COUNT],
	uint32_t tetrahedronIndex)
{
	TetrahedronCorner tetrahedronCorners[MARCHING_TETRAHEDRON_CORNER_COUNT] = {};

	gatherTetrahedronCorners(
		tetrahedronCorners,
		voxelCornerPositions,
		voxelCornerDensities,
		tetrahedronIndex);

	uint32_t insideCorners[MARCHING_TETRAHEDRON_CORNER_COUNT] = {};
	uint32_t outsideCorners[MARCHING_TETRAHEDRON_CORNER_COUNT] = {};

	uint32_t insideCount = 0;
	uint32_t outsideCount = 0;

	gatherInsideOutsideCornerIndices(
		insideCorners,
		insideCount,
		outsideCorners,
		outsideCount,
		tetrahedronCorners);

	if (insideCount == 0 || insideCount == MARCHING_TETRAHEDRON_CORNER_COUNT)
	{
		return;
	}

	const glm::vec3 desiredNormalDirection =
		getSurfaceDirectionFromSolidToAir(
			tetrahedronCorners,
			insideCorners,
			insideCount,
			outsideCorners,
			outsideCount);

	if (insideCount == 1)
	{
		const TetrahedronCorner& insideCorner =
			tetrahedronCorners[insideCorners[0]];

		const glm::vec3 pointA =
			getTetrahedronEdgeSurfacePoint(
				insideCorner,
				tetrahedronCorners[outsideCorners[0]]);

		const glm::vec3 pointB =
			getTetrahedronEdgeSurfacePoint(
				insideCorner,
				tetrahedronCorners[outsideCorners[1]]);

		const glm::vec3 pointC =
			getTetrahedronEdgeSurfacePoint(
				insideCorner,
				tetrahedronCorners[outsideCorners[2]]);

		addOrientedTriangle(
			mesh,
			pointA,
			pointB,
			pointC,
			desiredNormalDirection);
	}
	else if (insideCount == 3)
	{
		const TetrahedronCorner& outsideCorner =
			tetrahedronCorners[outsideCorners[0]];

		const glm::vec3 pointA =
			getTetrahedronEdgeSurfacePoint(
				outsideCorner,
				tetrahedronCorners[insideCorners[0]]);

		const glm::vec3 pointB =
			getTetrahedronEdgeSurfacePoint(
				outsideCorner,
				tetrahedronCorners[insideCorners[1]]);

		const glm::vec3 pointC =
			getTetrahedronEdgeSurfacePoint(
				outsideCorner,
				tetrahedronCorners[insideCorners[2]]);

		addOrientedTriangle(
			mesh,
			pointA,
			pointB,
			pointC,
			desiredNormalDirection);
	}
	else
	{
		assert(insideCount == 2);
		assert(outsideCount == 2);

		const TetrahedronCorner& insideCornerA =
			tetrahedronCorners[insideCorners[0]];

		const TetrahedronCorner& insideCornerB =
			tetrahedronCorners[insideCorners[1]];

		const TetrahedronCorner& outsideCornerA =
			tetrahedronCorners[outsideCorners[0]];

		const TetrahedronCorner& outsideCornerB =
			tetrahedronCorners[outsideCorners[1]];

		const glm::vec3 pointA =
			getTetrahedronEdgeSurfacePoint(
				insideCornerA,
				outsideCornerA);

		const glm::vec3 pointB =
			getTetrahedronEdgeSurfacePoint(
				insideCornerA,
				outsideCornerB);

		const glm::vec3 pointC =
			getTetrahedronEdgeSurfacePoint(
				insideCornerB,
				outsideCornerB);

		const glm::vec3 pointD =
			getTetrahedronEdgeSurfacePoint(
				insideCornerB,
				outsideCornerA);

		addOrientedTriangle(
			mesh,
			pointA,
			pointB,
			pointC,
			desiredNormalDirection);

		addOrientedTriangle(
			mesh,
			pointA,
			pointC,
			pointD,
			desiredNormalDirection);
	}
}

static void gatherVoxelCorners(
	glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT],
	float voxelCornerDensities[VOXEL_CORNER_COUNT],
	const Chunk& chunk,
	const VoxelCoord& voxelCoord)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));

	for (uint32_t cornerIndex = 0;
		cornerIndex < VOXEL_CORNER_COUNT;
		++cornerIndex)
	{
		const SampleCoord sampleCoord =
			getVoxelCornerSampleCoord(
				voxelCoord,
				cornerIndex);

		voxelCornerPositions[cornerIndex] =
			getVoxelCornerLocalPosition(
				voxelCoord,
				cornerIndex);

		voxelCornerDensities[cornerIndex] =
			getChunkDensitySample(
				chunk,
				sampleCoord);
	}
}

static void polygonizeVoxel(
	TetrahedraMesh& mesh,
	const Chunk& chunk,
	const VoxelCoord& voxelCoord)
{
	glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT] = {};
	float voxelCornerDensities[VOXEL_CORNER_COUNT] = {};

	gatherVoxelCorners(
		voxelCornerPositions,
		voxelCornerDensities,
		chunk,
		voxelCoord);

	for (uint32_t tetrahedronIndex = 0;
		tetrahedronIndex < MARCHING_TETRAHEDRA_PER_VOXEL;
		++tetrahedronIndex)
	{
		polygonizeTetrahedron(
			mesh,
			voxelCornerPositions,
			voxelCornerDensities,
			tetrahedronIndex);
	}
}

/***********************************************************
* Tetrahedra Builder Interface
************************************************************/

void clearTetrahedraMesh(
	TetrahedraMesh& mesh)
{
	mesh.vertices.clear();
	mesh.indices.clear();
}

bool buildTetrahedraMesh(
	TetrahedraMesh& mesh,
	const Chunk& chunk,
	const SurfaceChunk& surfaceChunk)
{
	clearTetrahedraMesh(mesh);

	assert(surfaceChunk.coord.x == chunk.coord.x);
	assert(surfaceChunk.coord.y == chunk.coord.y);
	assert(surfaceChunk.coord.z == chunk.coord.z);

	for (const SurfaceVoxel& surfaceVoxel : surfaceChunk.voxels)
	{
		polygonizeVoxel(
			mesh,
			chunk,
			surfaceVoxel.coord);
	}

	return true;
}
