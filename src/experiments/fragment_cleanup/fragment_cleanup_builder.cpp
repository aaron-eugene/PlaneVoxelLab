///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_builder.cpp
// ==========================================================
//
// Implements voxel-owned marching tetrahedra surface generation and
// exposed-face construction for the fragment cleanup experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"

#include "chunk/chunk.h"
#include "experiments/fragment_cleanup/fragment_cleanup_tables.h"
#include "fields/density_field.h"
#include "geometry/voxel_topology.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"
#include "surface_map/surface_map.h"
#include "terrain_render/terrain_tile_atlas.h"

#include <glm/common.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

#include <cassert>
#include <cstdint>

/***********************************************************
* Tetrahedra Building Constants
************************************************************/

static constexpr float TRIANGLE_AREA_EPSILON = 0.00001f;

/***********************************************************
* Terrain Material Constants
************************************************************/

static constexpr WorldMetricCoordinate
SAND_TO_GRASS_HEIGHT = { -6, 0.0f };

static constexpr WorldMetricCoordinate
GRASS_TO_ROCK_HEIGHT = { 2, 0.0f };

static constexpr WorldMetricCoordinate
ROCK_TO_SNOW_HEIGHT = { 16, 0.0f };

/***********************************************************
* Local Types
************************************************************/

struct TetrahedronCorner
{
	glm::vec3 position = {};
	float density = 0.0f;
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
* Terrain Material Helpers
************************************************************/

static TerrainTile getFragmentCleanupTerrainTile(
	const ChunkCoord& chunkCoord,
	const glm::vec3& position,
	const glm::vec3& normal)
{
	static constexpr float MATERIAL_HEIGHT_VARIATION = 4.0f;
	static constexpr float REFERENCE_NORMAL_Y = 0.7f;

	const WorldMetricCoordinate worldHeight =
		getWorldMetricCoordinate(
			chunkCoord.y,
			position.y);

	const float heightOffset =
		(REFERENCE_NORMAL_Y - normal.y) *
		MATERIAL_HEIGHT_VARIATION;

	const WorldMetricCoordinate classificationHeight =
		offsetWorldMetricCoordinate(
			worldHeight,
			heightOffset);

	if (compareWorldMetricCoordinates(
		classificationHeight,
		SAND_TO_GRASS_HEIGHT) < 0)
	{
		return TerrainTile::Sand;
	}

	if (compareWorldMetricCoordinates(
		classificationHeight,
		GRASS_TO_ROCK_HEIGHT) < 0)
	{
		return TerrainTile::Grass;
	}

	if (compareWorldMetricCoordinates(
		classificationHeight,
		ROCK_TO_SNOW_HEIGHT) < 0)
	{
		return TerrainTile::Rock;
	}

	return TerrainTile::Snow;
}

/***********************************************************
* Texture Coordinate Helpers
************************************************************/

static glm::vec2 getVoxelProjectedUv(
	const glm::vec3& position,
	const glm::vec3& normal,
	const VoxelCoord& voxelCoord)
{
	const glm::vec3 voxelLocalPosition =
		position - getVoxelLocalMin(voxelCoord);

	const glm::vec3 absoluteNormal =
		glm::abs(normal);

	glm::vec2 projectedUv = {};

	if (absoluteNormal.y >= absoluteNormal.x &&
		absoluteNormal.y >= absoluteNormal.z)
	{
		projectedUv = glm::vec2(
			voxelLocalPosition.x,
			voxelLocalPosition.z);
	}
	else if (absoluteNormal.x >= absoluteNormal.z)
	{
		projectedUv = glm::vec2(
			voxelLocalPosition.z,
			voxelLocalPosition.y);
	}
	else
	{
		projectedUv = glm::vec2(
			voxelLocalPosition.x,
			voxelLocalPosition.y);
	}

	projectedUv /= VOXEL_SIZE_METERS;

	return glm::clamp(
		projectedUv,
		glm::vec2(0.0f),
		glm::vec2(1.0f));
}

/***********************************************************
* Mesh Helpers
************************************************************/

static bool calculateTriangleNormal(
	glm::vec3& normal,
	const glm::vec3& pointA,
	const glm::vec3& pointB,
	const glm::vec3& pointC)
{
	const glm::vec3 edgeAB =
		pointB - pointA;

	const glm::vec3 edgeAC =
		pointC - pointA;

	const glm::vec3 crossProduct =
		glm::cross(
			edgeAB,
			edgeAC);

	const float lengthSquared =
		glm::dot(
			crossProduct,
			crossProduct);

	if (lengthSquared <=
		TRIANGLE_AREA_EPSILON *
		TRIANGLE_AREA_EPSILON)
	{
		normal = {};
		return false;
	}

	normal =
		glm::normalize(
			crossProduct);

	return true;
}

static glm::vec3 getFragmentCleanupNormalColor(
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
		(densityA - DENSITY_SURFACE_VALUE) / denominator;

	return positionA + t * (positionB - positionA);
}

static void addOrientedTriangle(
	FragmentCleanupMesh& mesh,
	const ChunkCoord& chunkCoord,
	const VoxelCoord& voxelCoord,
	const glm::vec3& pointA,
	const glm::vec3& pointB,
	const glm::vec3& pointC,
	const glm::vec3& desiredNormalDirection)
{
	glm::vec3 normal = {};

	if (!calculateTriangleNormal(
		normal,
		pointA,
		pointB,
		pointC))
	{
		return;
	}

	glm::vec3 finalPointB = pointB;
	glm::vec3 finalPointC = pointC;

	if (glm::dot(
		normal,
		desiredNormalDirection) < 0.0f)
	{
		finalPointB = pointC;
		finalPointC = pointB;
		normal = -normal;
	}

	const uint32_t baseIndex =
		static_cast<uint32_t>(
			mesh.vertices.size());

	const glm::vec3 color = glm::vec3(1.0f);

	const glm::vec3 triangleCentroid =
		(pointA + finalPointB + finalPointC) / 3.0f;

	const TerrainTile tile =
		getFragmentCleanupTerrainTile(
			chunkCoord,
			triangleCentroid,
			normal);

	const glm::vec2 uvA =
		getTerrainAtlasUv(
			getVoxelProjectedUv(
				pointA,
				normal,
				voxelCoord),
			tile);

	const glm::vec2 uvB =
		getTerrainAtlasUv(
			getVoxelProjectedUv(
				finalPointB,
				normal,
				voxelCoord),
			tile);

	const glm::vec2 uvC =
		getTerrainAtlasUv(
			getVoxelProjectedUv(
				finalPointC,
				normal,
				voxelCoord),
			tile);

	mesh.vertices.push_back(
		{ pointA, normal, color, uvA });

	mesh.vertices.push_back(
		{ finalPointB, normal, color, uvB });

	mesh.vertices.push_back(
		{ finalPointC, normal, color, uvC });

	mesh.indices.push_back(baseIndex + 0);
	mesh.indices.push_back(baseIndex + 1);
	mesh.indices.push_back(baseIndex + 2);
}

/***********************************************************
* Tetrahedra Helpers
************************************************************/

static void gatherInsideOutsideCornerIndices(
	uint32_t insideCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT],
	uint32_t& insideCount,
	uint32_t outsideCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT],
	uint32_t& outsideCount,
	const TetrahedronCorner tetrahedronCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT])
{
	insideCount = 0;
	outsideCount = 0;

	for (uint32_t cornerIndex = 0;
		cornerIndex < FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT;
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

	assert(insideCount + outsideCount == FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT);
}

static glm::vec3 getCornerGroupCentroid(
	const TetrahedronCorner tetrahedronCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT],
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

		assert(cornerIndex < FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT);

		centroid += tetrahedronCorners[cornerIndex].position;
	}

	return centroid / static_cast<float>(cornerCount);
}

static glm::vec3 getSurfaceDirectionFromSolidToAir(
	const TetrahedronCorner tetrahedronCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT],
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
	TetrahedronCorner tetrahedronCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT],
	const glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT],
	const float voxelCornerDensities[VOXEL_CORNER_COUNT],
	uint32_t tetrahedronIndex)
{
	assert(tetrahedronIndex < FRAGMENT_CLEANUP_TETRAHEDRA_PER_VOXEL);

	for (uint32_t tetrahedronCornerIndex = 0;
		tetrahedronCornerIndex < FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT;
		++tetrahedronCornerIndex)
	{
		const uint32_t voxelCornerIndex =
			FRAGMENT_CLEANUP_TETRAHEDRA_CORNERS
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
	FragmentCleanupMesh& mesh,
	const ChunkCoord& chunkCoord,
	const VoxelCoord& voxelCoord,
	const glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT],
	const float voxelCornerDensities[VOXEL_CORNER_COUNT],
	uint32_t tetrahedronIndex)
{
	TetrahedronCorner tetrahedronCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT] = {};

	gatherTetrahedronCorners(
		tetrahedronCorners,
		voxelCornerPositions,
		voxelCornerDensities,
		tetrahedronIndex);

	uint32_t insideCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT] = {};
	uint32_t outsideCorners[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT] = {};

	uint32_t insideCount = 0;
	uint32_t outsideCount = 0;

	gatherInsideOutsideCornerIndices(
		insideCorners,
		insideCount,
		outsideCorners,
		outsideCount,
		tetrahedronCorners);

	if (insideCount == 0 || insideCount == FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT)
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
			chunkCoord,
			voxelCoord,
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
			chunkCoord,
			voxelCoord,
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
			chunkCoord,
			voxelCoord,
			pointA,
			pointB,
			pointC,
			desiredNormalDirection);

		addOrientedTriangle(
			mesh,
			chunkCoord,
			voxelCoord,
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

/***********************************************************
* Exposed Face Helpers
************************************************************/

static void appendClippedFaceTriangle(
	FragmentCleanupMesh& mesh,
	const ChunkCoord& chunkCoord,
	const VoxelCoord& voxelCoord,
	const glm::vec3 positions[3],
	const float densities[3],
	const glm::vec3& outwardNormal)
{
	uint32_t insideCorners[3] = {};
	uint32_t outsideCorners[3] = {};

	uint32_t insideCount = 0;
	uint32_t outsideCount = 0;

	for (uint32_t cornerIndex = 0;
		cornerIndex < 3;
		++cornerIndex)
	{
		if (isDensityInside(densities[cornerIndex]))
		{
			insideCorners[insideCount++] = cornerIndex;
		}
		else
		{
			outsideCorners[outsideCount++] = cornerIndex;
		}
	}

	if (insideCount == 0)
	{
		return;
	}

	if (insideCount == 3)
	{
		addOrientedTriangle(
			mesh,
			chunkCoord,
			voxelCoord,
			positions[0],
			positions[1],
			positions[2],
			outwardNormal);

		return;
	}

	if (insideCount == 1)
	{
		const uint32_t inside = insideCorners[0];
		const uint32_t outsideA = outsideCorners[0];
		const uint32_t outsideB = outsideCorners[1];

		const glm::vec3 pointA =
			interpolateSurfacePoint(
				positions[inside],
				densities[inside],
				positions[outsideA],
				densities[outsideA]);

		const glm::vec3 pointB =
			interpolateSurfacePoint(
				positions[inside],
				densities[inside],
				positions[outsideB],
				densities[outsideB]);

		addOrientedTriangle(
			mesh,
			chunkCoord,
			voxelCoord,
			positions[inside],
			pointA,
			pointB,
			outwardNormal);

		return;
	}

	assert(insideCount == 2);
	assert(outsideCount == 1);

	const uint32_t insideA = insideCorners[0];
	const uint32_t insideB = insideCorners[1];
	const uint32_t outside = outsideCorners[0];

	const glm::vec3 pointA =
		interpolateSurfacePoint(
			positions[insideA],
			densities[insideA],
			positions[outside],
			densities[outside]);

	const glm::vec3 pointB =
		interpolateSurfacePoint(
			positions[insideB],
			densities[insideB],
			positions[outside],
			densities[outside]);

	addOrientedTriangle(
		mesh,
		chunkCoord,
		voxelCoord,
		positions[insideA],
		positions[insideB],
		pointB,
		outwardNormal);

	addOrientedTriangle(
		mesh,
		chunkCoord,
		voxelCoord,
		positions[insideA],
		pointB,
		pointA,
		outwardNormal);
}

/***********************************************************
* Exposed Face Building
************************************************************/

void appendFragmentCleanupExposedFace(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const VoxelCoord& voxelCoord,
	uint32_t faceIndex)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));
	assert(faceIndex < VOXEL_FACE_COUNT);

	const VoxelFace face =
		getVoxelFace(faceIndex);

	glm::vec3 positions[4] = {};
	float densities[4] = {};

	for (uint32_t cornerIndex = 0;
		cornerIndex < 4;
		++cornerIndex)
	{
		const uint32_t voxelCornerIndex =
			face.corners[cornerIndex];

		const SampleCoord sampleCoord =
			getVoxelCornerSampleCoord(
				voxelCoord,
				voxelCornerIndex);

		positions[cornerIndex] =
			getVoxelCornerLocalPosition(
				voxelCoord,
				voxelCornerIndex);

		densities[cornerIndex] =
			getChunkDensitySample(
				chunk,
				sampleCoord);
	}

	const uint32_t triangleCorners[2][3] =
	{
		{ 0, 1, 2 },
		{ 0, 2, 3 }
	};

	const glm::vec3 outwardNormal =
		glm::normalize(
			glm::cross(
				positions[1] - positions[0],
				positions[2] - positions[0]));

	for (uint32_t triangleIndex = 0;
		triangleIndex < 2;
		++triangleIndex)
	{
		glm::vec3 trianglePositions[3] = {};
		float triangleDensities[3] = {};

		for (uint32_t cornerIndex = 0;
			cornerIndex < 3;
			++cornerIndex)
		{
			const uint32_t faceCornerIndex =
				triangleCorners[triangleIndex][cornerIndex];

			trianglePositions[cornerIndex] =
				positions[faceCornerIndex];

			triangleDensities[cornerIndex] =
				densities[faceCornerIndex];
		}

		appendClippedFaceTriangle(
			mesh,
			chunk.coord,
			voxelCoord,
			trianglePositions,
			triangleDensities,
			outwardNormal);
	}
}

/***********************************************************
* Fragment Cleanup Mesh Interface
************************************************************/

void clearFragmentCleanupMesh(
	FragmentCleanupMesh& mesh)
{
	mesh.vertices.clear();
	mesh.indices.clear();
	mesh.voxelRanges.clear();
}

void appendFragmentCleanupVoxel(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const VoxelCoord& voxelCoord)
{
	const uint32_t firstIndex =
		static_cast<uint32_t>(
			mesh.indices.size());

	glm::vec3 voxelCornerPositions[VOXEL_CORNER_COUNT] = {};
	float voxelCornerDensities[VOXEL_CORNER_COUNT] = {};

	gatherVoxelCorners(
		voxelCornerPositions,
		voxelCornerDensities,
		chunk,
		voxelCoord);

	for (uint32_t tetrahedronIndex = 0;
		tetrahedronIndex < FRAGMENT_CLEANUP_TETRAHEDRA_PER_VOXEL;
		++tetrahedronIndex)
	{
		polygonizeTetrahedron(
			mesh,
			chunk.coord,
			voxelCoord,
			voxelCornerPositions,
			voxelCornerDensities,
			tetrahedronIndex);
	}

	const uint32_t indexCount =
		static_cast<uint32_t>(
			mesh.indices.size()) - firstIndex;

	if (indexCount > 0)
	{
		FragmentCleanupVoxelRange voxelRange = {};

		voxelRange.coord = voxelCoord;
		voxelRange.firstIndex = firstIndex;
		voxelRange.indexCount = indexCount;

		mesh.voxelRanges.push_back(
			voxelRange);
	}
}

void buildFragmentCleanupMesh(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const DiscoveredSurfaceChunk& surfaceChunk)
{
	clearFragmentCleanupMesh(mesh);

	assert(surfaceChunk.coord.x == chunk.coord.x);
	assert(surfaceChunk.coord.y == chunk.coord.y);
	assert(surfaceChunk.coord.z == chunk.coord.z);

	for (const SurfaceVoxel& surfaceVoxel : surfaceChunk.voxels)
	{
		appendFragmentCleanupVoxel(
			mesh,
			chunk,
			surfaceVoxel.coord);
	}
}
