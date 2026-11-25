///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.cpp
// ============================================================
//
// Implements the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"

#include "chunk/chunk.h"
#include "experiments/fragment_cleanup/fragment_cleanup_adjacency.h"
#include "experiments/fragment_cleanup/fragment_cleanup_blend_distance.h"
#include "experiments/fragment_cleanup/fragment_cleanup_cross_chunk.h"
#include "experiments/fragment_cleanup/fragment_cleanup_material_boundaries.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world.h"
#include "math/noise.h"
#include "renderer/gpu_mesh.h"
#include "renderer/gpu_noise_domain.h"
#include "renderer/gpu_surface_normals.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"
#include "surface_map/surface_map.h"
#include "terrain_edit/terrain_edit.h"
#include "terrain_render/terrain_render_resources.h"

#include <imgui/imgui.h>

#include <glad/glad.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

#ifndef NDEBUG

/***********************************************************
* CPU Heightmap Normal Reference
************************************************************/

static constexpr float HEIGHTMAP_NORMAL_GRADIENT_STEP = 0.1f;
static constexpr float HEIGHTMAP_NORMAL_TOLERANCE = 0.01f;

static float sampleFragmentCleanupCpuHeightmapNoise(
	const WorldMetricCoordinate& worldX,
	const WorldMetricCoordinate& worldZ,
	const HeightmapDensityField& heightmap)
{
	return sampleFractalValueNoise2d(
		worldX.wholeMeters,
		worldZ.wholeMeters,
		worldX.fractionalMeter,
		worldZ.fractionalMeter,
		heightmap.baseScale,
		heightmap.octaveCount,
		heightmap.persistence,
		heightmap.lacunarity,
		heightmap.seed);
}

static glm::vec3 sampleFragmentCleanupCpuHeightmapNormal(
	const WorldPosition& worldPosition,
	const HeightmapDensityField& heightmap)
{
	const WorldMetricCoordinate worldX =
		getWorldMetricCoordinate(
			worldPosition.chunk.x,
			worldPosition.localPosition.x);

	const WorldMetricCoordinate worldZ =
		getWorldMetricCoordinate(
			worldPosition.chunk.z,
			worldPosition.localPosition.z);

	const WorldMetricCoordinate leftX =
		offsetWorldMetricCoordinate(
			worldX,
			-HEIGHTMAP_NORMAL_GRADIENT_STEP);

	const WorldMetricCoordinate rightX =
		offsetWorldMetricCoordinate(
			worldX,
			HEIGHTMAP_NORMAL_GRADIENT_STEP);

	const WorldMetricCoordinate backZ =
		offsetWorldMetricCoordinate(
			worldZ,
			-HEIGHTMAP_NORMAL_GRADIENT_STEP);

	const WorldMetricCoordinate frontZ =
		offsetWorldMetricCoordinate(
			worldZ,
			HEIGHTMAP_NORMAL_GRADIENT_STEP);

	const float left =
		sampleFragmentCleanupCpuHeightmapNoise(
			leftX, worldZ, heightmap);

	const float right =
		sampleFragmentCleanupCpuHeightmapNoise(
			rightX, worldZ, heightmap);

	const float back =
		sampleFragmentCleanupCpuHeightmapNoise(
			worldX, backZ, heightmap);

	const float front =
		sampleFragmentCleanupCpuHeightmapNoise(
			worldX, frontZ, heightmap);

	const float derivativeScale =
		heightmap.amplitude *
		(0.5f / HEIGHTMAP_NORMAL_GRADIENT_STEP);

	return glm::normalize(glm::vec3(
		-(right - left) * derivativeScale,
		1.0f,
		-(front - back) * derivativeScale));
}

/***********************************************************
* Triangle Adjacency Validation
************************************************************/

static bool isSurfaceIntersectionKeyEqual(
	const SurfaceIntersectionKey& a,
	const SurfaceIntersectionKey& b)
{
	return
		a.sampleA.x == b.sampleA.x &&
		a.sampleA.y == b.sampleA.y &&
		a.sampleA.z == b.sampleA.z &&
		a.sampleB.x == b.sampleB.x &&
		a.sampleB.y == b.sampleB.y &&
		a.sampleB.z == b.sampleB.z;
}

static void validateFragmentCleanupAdjacency(
	const FragmentCleanupMesh& mesh)
{
	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.adjacency.size();
		++triangleIndex)
	{
		const FragmentCleanupTriangleAdjacency& adjacency =
			mesh.adjacency[triangleIndex];

		const SurfaceTriangleKeys& triangleKeys =
			mesh.topologyTriangles[triangleIndex].keys;

		for (uint32_t edgeIndex = 0;
			edgeIndex < 3;
			++edgeIndex)
		{
			const uint32_t neighborIndex =
				adjacency.neighbors[edgeIndex];

			if (neighborIndex == UINT32_MAX)
			{
				continue;
			}

			assert(neighborIndex < mesh.adjacency.size());
			assert(neighborIndex != triangleIndex);

			const FragmentCleanupTriangleAdjacency& neighbor =
				mesh.adjacency[neighborIndex];

			const SurfaceTriangleKeys& neighborKeys =
				mesh.topologyTriangles[neighborIndex].keys;

			const SurfaceIntersectionKey& edgeA =
				triangleKeys.vertices[edgeIndex];

			const SurfaceIntersectionKey& edgeB =
				triangleKeys.vertices[(edgeIndex + 1) % 3];

			bool reciprocal = false;

			for (uint32_t neighborEdgeIndex = 0;
				neighborEdgeIndex < 3;
				++neighborEdgeIndex)
			{
				if (neighbor.neighbors[neighborEdgeIndex] !=
					triangleIndex)
				{
					continue;
				}

				const SurfaceIntersectionKey& neighborA =
					neighborKeys.vertices[neighborEdgeIndex];

				const SurfaceIntersectionKey& neighborB =
					neighborKeys.vertices[(neighborEdgeIndex + 1) % 3];

				const bool matchingEdge =
					(isSurfaceIntersectionKeyEqual(edgeA, neighborA) &&
						isSurfaceIntersectionKeyEqual(edgeB, neighborB)) ||
					(isSurfaceIntersectionKeyEqual(edgeA, neighborB) &&
						isSurfaceIntersectionKeyEqual(edgeB, neighborA));

				assert(matchingEdge);

				reciprocal = true;
				break;
			}

			assert(reciprocal);
		}
	}
}

static void validateFragmentCleanupRenderMapping(
	const FragmentCleanupMesh& mesh)
{
	assert(mesh.indices.size() % 3 == 0);

	const size_t renderTriangleCount =
		mesh.indices.size() / 3;

	std::vector<bool> mappedTriangles(
		renderTriangleCount,
		false);

	size_t mappedCount = 0;

	for (const FragmentCleanupTopologyTriangle& triangle :
		mesh.topologyTriangles)
	{
		if (triangle.renderTriangleIndex == UINT32_MAX)
		{
			assert(!triangle.hasMaterial);
			continue;
		}

		assert(triangle.hasMaterial);

		const uint32_t renderIndex =
			triangle.renderTriangleIndex;

		assert(renderIndex < renderTriangleCount);
		assert(!mappedTriangles[renderIndex]);

		mappedTriangles[renderIndex] = true;
		++mappedCount;
	}

	assert(mappedCount == renderTriangleCount);
}

static void validateFragmentCleanupCrossChunkConnections(
	const FragmentCleanupExperiment& experiment)
{
	for (const FragmentCleanupCrossChunkConnection& connection :
		experiment.crossChunkConnections)
	{
		const FragmentCleanupCrossChunkEndpoint& a =
			connection.a;

		const FragmentCleanupCrossChunkEndpoint& b =
			connection.b;

		assert(a.meshIndex < experiment.meshes.size());
		assert(b.meshIndex < experiment.meshes.size());
		assert(a.meshIndex != b.meshIndex);

		const FragmentCleanupRenderMesh& meshA =
			experiment.meshes[a.meshIndex];

		const FragmentCleanupRenderMesh& meshB =
			experiment.meshes[b.meshIndex];

		assert(a.triangleIndex <
			meshA.cpuMesh.topologyTriangles.size());

		assert(b.triangleIndex <
			meshB.cpuMesh.topologyTriangles.size());

		assert(a.edgeIndex < 3);
		assert(b.edgeIndex < 3);

		assert(meshA.cpuMesh.adjacency[a.triangleIndex]
			.neighbors[a.edgeIndex] == UINT32_MAX);

		assert(meshB.cpuMesh.adjacency[b.triangleIndex]
			.neighbors[b.edgeIndex] == UINT32_MAX);

		const FragmentCleanupTopologyTriangle& triangleA =
			meshA.cpuMesh.topologyTriangles[a.triangleIndex];

		const FragmentCleanupTopologyTriangle& triangleB =
			meshB.cpuMesh.topologyTriangles[b.triangleIndex];

		const glm::vec3 chunkOffset =
			getChunkRelativePosition(
				meshB.coord,
				meshA.coord);

		const glm::vec3 a0 =
			triangleA.positions[a.edgeIndex];

		const glm::vec3 a1 =
			triangleA.positions[(a.edgeIndex + 1) % 3];

		const glm::vec3 b0 =
			triangleB.positions[b.edgeIndex] +
			chunkOffset;

		const glm::vec3 b1 =
			triangleB.positions[(b.edgeIndex + 1) % 3] +
			chunkOffset;

		const float tolerance = 0.0001f;

		const bool sameDirection =
			glm::length(a0 - b0) <= tolerance &&
			glm::length(a1 - b1) <= tolerance;

		const bool oppositeDirection =
			glm::length(a0 - b1) <= tolerance &&
			glm::length(a1 - b0) <= tolerance;

		assert(sameDirection || oppositeDirection);
	}
}

#endif

/***********************************************************
* Render-Mesh Helpers
************************************************************/

static void destroyFragmentCleanupRenderMesh(
	FragmentCleanupRenderMesh& renderMesh)
{
	destroyGpuSurfaceNormalBuffer(
		renderMesh.smoothNormals);

	destroyGpuMesh(
		renderMesh.gpuMesh);

	renderMesh = {};
}

static void destroyFragmentCleanupExperimentMeshes(
	FragmentCleanupExperiment& experiment)
{
	experiment.crossChunkConnections.clear();
	
	for (FragmentCleanupRenderMesh& renderMesh :
		experiment.meshes)
	{
		destroyFragmentCleanupRenderMesh(
			renderMesh);
	}

	experiment.meshes.clear();
	experiment.debugStats = {};

	experiment.blendDistances.clear();
	experiment.blendBoundarySeeds.clear();
}

static bool uploadFragmentCleanupRenderMesh(
	FragmentCleanupRenderMesh& renderMesh)
{
	assert(renderMesh.gpuMesh.vertexArray == 0);
	assert(renderMesh.gpuMesh.vertexBuffer == 0);
	assert(renderMesh.gpuMesh.indexBuffer == 0);

	assert(renderMesh.smoothNormals.handle == 0);
	assert(!renderMesh.hasSmoothNormals);

	assert(
		renderMesh.cpuMesh.vertices.empty() ==
		renderMesh.cpuMesh.indices.empty());

	if (renderMesh.cpuMesh.vertices.empty())
	{
		return true;
	}

	assert(
		renderMesh.cpuMesh.vertices.size() <=
		std::numeric_limits<uint32_t>::max());

	assert(
		renderMesh.cpuMesh.indices.size() <=
		std::numeric_limits<uint32_t>::max());

	const uint32_t vertexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.vertices.size());

	const uint32_t indexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.indices.size());

	return createStandardGpuMesh(
		renderMesh.gpuMesh,
		renderMesh.cpuMesh.vertices.data(),
		vertexCount,
		renderMesh.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles);
}

static bool generateFragmentCleanupSphereNormals(
	FragmentCleanupRenderMesh& renderMesh,
	const GpuSurfaceNormalGenerator& generator,
	const SphereDensityField& sphere)
{
	assert(renderMesh.gpuMesh.vertexArray != 0);
	assert(renderMesh.gpuMesh.vertexBuffer != 0);
	assert(renderMesh.gpuMesh.vertexCount > 0);

	assert(renderMesh.smoothNormals.handle == 0);
	assert(!renderMesh.hasSmoothNormals);

	const uint32_t vertexCount =
		renderMesh.gpuMesh.vertexCount;

	if (!createGpuSurfaceNormalBuffer(
		renderMesh.smoothNormals,
		vertexCount))
	{
		return false;
	}

	generateGpuSphereSurfaceNormals(
		generator,
		renderMesh.gpuMesh,
		renderMesh.smoothNormals,
		renderMesh.coord,
		sphere);

	attachGpuMeshSmoothNormals(
		renderMesh.gpuMesh,
		renderMesh.smoothNormals.handle);

	renderMesh.hasSmoothNormals = true;

	return true;
}

static glm::mat4 getChunkModelMatrix(
	const ChunkCoord& chunkCoord,
	const ChunkCoord& renderOriginChunk)
{
	return glm::translate(
		glm::mat4(1.0f),
		getChunkRelativePosition(
			chunkCoord,
			renderOriginChunk));
}

static bool generateFragmentCleanupHeightmapNormals(
	FragmentCleanupRenderMesh& renderMesh,
	const GpuSurfaceNormalGenerator& generator)
{
	assert(renderMesh.gpuMesh.vertexArray != 0);
	assert(renderMesh.gpuMesh.vertexBuffer != 0);
	assert(renderMesh.gpuMesh.vertexCount > 0);

	assert(renderMesh.smoothNormals.handle == 0);
	assert(!renderMesh.hasSmoothNormals);

	if (!createGpuSurfaceNormalBuffer(
		renderMesh.smoothNormals,
		renderMesh.gpuMesh.vertexCount))
	{
		return false;
	}

	generateGpuHeightmapSurfaceNormals(
		generator,
		renderMesh.gpuMesh,
		renderMesh.smoothNormals,
		renderMesh.coord);

	attachGpuMeshSmoothNormals(
		renderMesh.gpuMesh,
		renderMesh.smoothNormals.handle);

	renderMesh.hasSmoothNormals = true;

	return true;
}

/***********************************************************
* Debug-Stats Helpers
************************************************************/

static void updateFragmentCleanupDebugStats(
	FragmentCleanupExperiment& experiment)
{
	FragmentCleanupDebugStats& stats =
		experiment.debugStats;

	stats = {};

	stats.meshCount =
		experiment.meshes.size();

	for (const FragmentCleanupRenderMesh& renderMesh :
		experiment.meshes)
	{
		stats.voxelRangeCount +=
			renderMesh.cpuMesh.voxelRanges.size();
		
		stats.vertexCount +=
			renderMesh.cpuMesh.vertices.size();

		stats.indexCount +=
			renderMesh.cpuMesh.indices.size();
	}
}

#ifndef NDEBUG

/***********************************************************
* Mesh Building Helpers
************************************************************/

static void visualizeFragmentCleanupBlendDistances(
	FragmentCleanupMesh& mesh,
	const std::vector<float>& distances,
	float maxDistance)
{
	assert(distances.size() == mesh.topologyTriangles.size());
	assert(maxDistance > 0.0f);

	const glm::vec3 boundaryColor =
		glm::vec3(1.0f, 0.15f, 0.15f);

	const glm::vec3 normalColor =
		glm::vec3(1.0f);

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.topologyTriangles.size();
		++triangleIndex)
	{
		const FragmentCleanupTopologyTriangle& triangle =
			mesh.topologyTriangles[triangleIndex];

		if (triangle.renderTriangleIndex == UINT32_MAX)
		{
			continue;
		}

		const float distance =
			distances[triangleIndex];

		if (!std::isfinite(distance))
		{
			continue;
		}

		const float blend =
			std::clamp(
				distance / maxDistance,
				0.0f,
				1.0f);

		const glm::vec3 color =
			glm::mix(
				boundaryColor,
				normalColor,
				blend);

		const uint32_t firstIndex =
			triangle.renderTriangleIndex * 3;

		assert(firstIndex + 2 < mesh.indices.size());

		for (uint32_t corner = 0; corner < 3; ++corner)
		{
			const uint32_t vertexIndex =
				mesh.indices[firstIndex + corner];

			assert(vertexIndex < mesh.vertices.size());

			mesh.vertices[vertexIndex].color = color;
		}
	}
}

static bool doesTriangleBorderRejectedTriangle(
	const FragmentCleanupMesh& mesh,
	uint32_t triangleIndex)
{
	assert(
		triangleIndex <
		mesh.topologyTriangles.size());

	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());

	const FragmentCleanupTriangleAdjacency& adjacency =
		mesh.adjacency[triangleIndex];

	for (uint32_t edgeIndex = 0;
		edgeIndex < 3;
		++edgeIndex)
	{
		const uint32_t neighborIndex =
			adjacency.neighbors[edgeIndex];

		if (neighborIndex == UINT32_MAX)
		{
			continue;
		}

		assert(
			neighborIndex <
			mesh.topologyTriangles.size());

		const FragmentCleanupTopologyTriangle& neighbor =
			mesh.topologyTriangles[neighborIndex];

		if (neighbor.renderTriangleIndex == UINT32_MAX)
		{
			return true;
		}
	}

	return false;
}

static void visualizeRejectedTriangleBorders(
	FragmentCleanupMesh& mesh)
{
	assert(mesh.indices.size() % 3 == 0);

	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());
	
	const glm::vec3 rejectedNeighborTint =
		glm::vec3(0.35f, 0.80f, 1.00f);

	static constexpr float TINT_STRENGTH = 0.35f;
	
	/*
	const glm::vec3 rejectedNeighborTint =
		glm::vec3(0.1f, 0.3f, 1.0f);

	static constexpr float TINT_STRENGTH = 1.0f;
	*/

	uint32_t highlightedTriangleCount = 0;

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.topologyTriangles.size();
		++triangleIndex)
	{
		const FragmentCleanupTopologyTriangle& triangle =
			mesh.topologyTriangles[triangleIndex];

		if (triangle.renderTriangleIndex == UINT32_MAX)
		{
			continue;
		}

		if (!doesTriangleBorderRejectedTriangle(
			mesh,
			triangleIndex))
		{
			continue;
		}

		const uint32_t firstIndex =
			triangle.renderTriangleIndex * 3;

		assert(firstIndex + 2 < mesh.indices.size());

		for (uint32_t corner = 0;
			corner < 3;
			++corner)
		{
			const uint32_t vertexIndex =
				mesh.indices[firstIndex + corner];

			assert(vertexIndex < mesh.vertices.size());

			glm::vec3& color =
				mesh.vertices[vertexIndex].color;

			color = glm::mix(
				color,
				rejectedNeighborTint,
				TINT_STRENGTH);
		}

		++highlightedTriangleCount;
	}

	std::printf(
		"Fragment cleanup rejected-border visualization:\n"
		"  Highlighted triangles: %u\n",
		highlightedTriangleCount);
}

#endif

static void buildEditedTetrahedraMesh(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const DiscoveredSurfaceChunk& surfaceChunk,
	const TerrainEditChunk* editChunk)
{
	clearFragmentCleanupMesh(mesh);

	for (const SurfaceVoxel& surfaceVoxel :
		surfaceChunk.voxels)
	{
		if (editChunk != nullptr &&
			isVoxelExcavated(
				*editChunk,
				surfaceVoxel.coord))
		{
			continue;
		}

		appendFragmentCleanupVoxel(
			mesh,
			chunk,
			surfaceVoxel.coord);
	}
}

/***********************************************************
* GPU Heightmap Configuration
************************************************************/

static bool prepareFragmentCleanupGpuHeightmap(
	const HeightmapDensityField& heightmap,
	GpuHeightmapNoiseConfiguration& configuration)
{
	const GpuNoiseDomain domain =
		getFullGpuNoiseDomain();

	const GpuNoiseDomainResult result =
		prepareGpuHeightmapNoiseConfiguration(
			heightmap,
			domain,
			configuration);

	if (result.failure != GpuNoiseDomainFailure::None)
	{
		std::printf(
			"Unsupported GPU heightmap configuration.\n"
			"  Octave: %u\n"
			"  Reason: %s\n",
			result.failedOctave,
			getGpuNoiseDomainFailureDescription(
				result.failure));

		assert(false &&
			"Unsupported GPU heightmap configuration");

		return false;
	}

	return true;
}

#ifndef NDEBUG

/***********************************************************
* World Blend Distance Visualization
************************************************************/

static void visualizeFragmentCleanupBoundarySeeds(
	FragmentCleanupMesh& mesh,
	const std::vector<float>& distances,
	const std::vector<uint8_t>& boundarySeeds)
{
	assert(distances.size() == mesh.topologyTriangles.size());
	assert(boundarySeeds.size() == mesh.topologyTriangles.size());

	const glm::vec3 seedColor =
		glm::vec3(0.0f, 1.0f, 1.0f);

	const glm::vec3 propagatedColor =
		glm::vec3(1.0f, 0.0f, 1.0f);

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.topologyTriangles.size();
		++triangleIndex)
	{
		const FragmentCleanupTopologyTriangle& triangle =
			mesh.topologyTriangles[triangleIndex];

		if (triangle.renderTriangleIndex == UINT32_MAX)
		{
			continue;
		}

		const bool isSeed =
			boundarySeeds[triangleIndex] != 0;

		const bool isReached =
			std::isfinite(distances[triangleIndex]);

		if (!isSeed && !isReached)
		{
			continue;
		}

		const glm::vec3 color =
			isSeed ? seedColor : propagatedColor;

		const uint32_t firstIndex =
			triangle.renderTriangleIndex * 3;

		assert(firstIndex + 2 < mesh.indices.size());

		for (uint32_t corner = 0; corner < 3; ++corner)
		{
			const uint32_t vertexIndex =
				mesh.indices[firstIndex + corner];

			assert(vertexIndex < mesh.vertices.size());

			mesh.vertices[vertexIndex].color = color;
		}
	}
}

/***********************************************************
* Material Blend Debug Update
************************************************************/

static const char* getFragmentCleanupMaterialName(
	TerrainTile material)
{
	switch (material)
	{
	case TerrainTile::Grass:
		return "Grass";

	case TerrainTile::Sand:
		return "Sand";

	case TerrainTile::Rock:
		return "Rock";

	case TerrainTile::Snow:
		return "Snow";

	default:
		return "Unknown";
	}
}


/***********************************************************
* Material Blend Debug Visualization
************************************************************/

static glm::vec3 getFragmentCleanupMaterialDebugColor(
	TerrainTile material)
{
	switch (material)
	{
	case TerrainTile::Grass:
		return glm::vec3(0.15f, 0.75f, 0.20f);

	case TerrainTile::Sand:
		return glm::vec3(1.00f, 0.75f, 0.15f);

	case TerrainTile::Rock:
		return glm::vec3(0.45f, 0.50f, 0.60f);

	case TerrainTile::Snow:
		return glm::vec3(0.55f, 0.85f, 1.00f);

	default:
		return glm::vec3(1.0f, 0.0f, 1.0f);
	}
}

static void applyFragmentCleanupBlendDebugColors(
	FragmentCleanupExperiment& experiment)
{
	const FragmentCleanupBlendDebugSettings& settings =
		experiment.blendDebug;

	const bool normalMode =
		settings.mode == FragmentCleanupDebugMode::Normal;

	experiment.surfaceSettings.visualizeDebugColors =
		!normalMode;

	const float maxDistance =
		settings.baseDistanceMeters *
		settings.distanceScale;

	const glm::vec3 normalColor(1.0f);
	const glm::vec3 unreachedColor(0.15f);
	const glm::vec3 seedColor(0.0f, 1.0f, 1.0f);
	const glm::vec3 propagatedColor(1.0f, 0.0f, 1.0f);

	for (size_t meshIndex = 0;
		meshIndex < experiment.meshes.size();
		++meshIndex)
	{
		FragmentCleanupRenderMesh& renderMesh =
			experiment.meshes[meshIndex];

		FragmentCleanupMesh& mesh =
			renderMesh.cpuMesh;

		const bool hasDistances =
			meshIndex < experiment.blendDistances.size() &&
			meshIndex < experiment.blendBoundarySeeds.size();

		//--------------------------------------------------
		// Set Default Colors
		//--------------------------------------------------

		const glm::vec3 defaultColor =
			normalMode ? normalColor : unreachedColor;

		for (StandardVertex& vertex : mesh.vertices)
		{
			vertex.color = defaultColor;
		}

		//--------------------------------------------------
		// Color Rendered Triangles
		//--------------------------------------------------

		for (uint32_t triangleIndex = 0;
			triangleIndex < mesh.topologyTriangles.size();
			++triangleIndex)
		{
			const FragmentCleanupTopologyTriangle& triangle =
				mesh.topologyTriangles[triangleIndex];

			if (triangle.renderTriangleIndex == UINT32_MAX)
			{
				continue;
			}

			glm::vec3 color = defaultColor;

			const float distance =
				hasDistances
				? experiment.blendDistances[meshIndex][triangleIndex]
				: std::numeric_limits<float>::infinity();

			const bool reached =
				std::isfinite(distance);

			const bool isSeed =
				hasDistances &&
				experiment.blendBoundarySeeds[meshIndex][triangleIndex] != 0;

			switch (settings.mode)
			{
			case FragmentCleanupDebugMode::Normal:
				color = normalColor;
				break;

			case FragmentCleanupDebugMode::Materials:
				color = getFragmentCleanupMaterialDebugColor(
					triangle.material);
				break;

			case FragmentCleanupDebugMode::BoundarySeeds:
				if (reached)
				{
					color = isSeed
						? seedColor
						: propagatedColor;
				}
				break;

			case FragmentCleanupDebugMode::BlendDistance:
				if (reached && maxDistance > 0.0f)
				{
					const float blend =
						std::clamp(
							distance / maxDistance,
							0.0f,
							1.0f);

					const glm::vec3 nearColor(
						1.0f, 0.15f, 0.05f);

					const glm::vec3 farColor(
						0.10f, 0.35f, 1.0f);

					color = glm::mix(
						nearColor,
						farColor,
						blend);
				}
				break;
			}

			const uint32_t firstIndex =
				triangle.renderTriangleIndex * 3;

			assert(firstIndex + 2 < mesh.indices.size());

			for (uint32_t corner = 0;
				corner < 3;
				++corner)
			{
				const uint32_t vertexIndex =
					mesh.indices[firstIndex + corner];

				assert(vertexIndex < mesh.vertices.size());

				mesh.vertices[vertexIndex].color = color;
			}
		}

		//--------------------------------------------------
		// Update Existing GPU Vertex Buffer
		//--------------------------------------------------

		if (renderMesh.gpuMesh.vertexBuffer != 0 &&
			!mesh.vertices.empty())
		{
			assert(
				renderMesh.gpuMesh.vertexCount ==
				mesh.vertices.size());

			const GLsizeiptr dataSize =
				static_cast<GLsizeiptr>(
					mesh.vertices.size() *
					sizeof(StandardVertex));

			glNamedBufferSubData(
				renderMesh.gpuMesh.vertexBuffer,
				0,
				dataSize,
				mesh.vertices.data());
		}
	}
}

static void updateFragmentCleanupBlendDebug(
	FragmentCleanupExperiment& experiment)
{
	const FragmentCleanupBlendDebugSettings& settings =
		experiment.blendDebug;

	assert(settings.sourceMaterial != settings.targetMaterial);
	assert(settings.baseDistanceMeters > 0.0f);
	assert(settings.distanceScale >= 0.0f);

	const float maxDistanceMeters =
		settings.baseDistanceMeters *
		settings.distanceScale;

	experiment.blendDistances.clear();
	experiment.blendBoundarySeeds.clear();

	uint32_t seedCount = 0;

	if (maxDistanceMeters > 0.0f)
	{
		seedCount =
			calculateFragmentCleanupWorldBlendDistances(
				experiment.blendDistances,
				experiment.blendBoundarySeeds,
				experiment.meshes,
				experiment.crossChunkConnections,
				settings.targetMaterial,
				settings.sourceMaterial,
				maxDistanceMeters);
	}

	//--------------------------------------------------
	// Collect Distance Statistics
	//--------------------------------------------------

	uint64_t reachedTriangleCount = 0;
	uint32_t reachedChunkCount = 0;

	for (const std::vector<float>& chunkDistances :
		experiment.blendDistances)
	{
		bool chunkReached = false;

		for (float distance : chunkDistances)
		{
			if (!std::isfinite(distance))
			{
				continue;
			}

			++reachedTriangleCount;
			chunkReached = true;
		}

		if (chunkReached)
		{
			++reachedChunkCount;
		}
	}

	//--------------------------------------------------
	// Apply Current Visualization
	//--------------------------------------------------

	applyFragmentCleanupBlendDebugColors(
		experiment);

	//--------------------------------------------------
	// Report Results
	//--------------------------------------------------

	std::printf(
		"Fragment cleanup material blend debug:\n"
		"  Source: %s\n"
		"  Target: %s\n"
		"  Base distance: %.2f m\n"
		"  Distance scale: %.2f\n"
		"  Effective distance: %.2f m\n"
		"  Boundary seeds: %u\n"
		"  Reached triangles: %llu\n"
		"  Reached chunks: %u\n",
		getFragmentCleanupMaterialName(
			settings.sourceMaterial),
		getFragmentCleanupMaterialName(
			settings.targetMaterial),
		settings.baseDistanceMeters,
		settings.distanceScale,
		maxDistanceMeters,
		seedCount,
		static_cast<unsigned long long>(
			reachedTriangleCount),
		reachedChunkCount);
}

#endif

/***********************************************************
* Fragment Cleanup Lifecycle
************************************************************/

bool initializeFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits)
{
	assert(experiment.meshes.empty());
	assert(experiment.normalGenerator.sphere.program.handle == 0);

	experiment = {};

	if (!initializeGpuSurfaceNormalGenerator(
		experiment.normalGenerator))
	{
		shutdownFragmentCleanupExperiment(
			experiment);

		return false;
	}

	if (!rebuildFragmentCleanupExperiment(
		experiment,
		world,
		edits))
	{
		shutdownFragmentCleanupExperiment(
			experiment);

		return false;
	}

	return true;
}

void shutdownFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment)
{
	destroyFragmentCleanupExperimentMeshes(
		experiment);

	shutdownGpuSurfaceNormalGenerator(
		experiment.normalGenerator);

	experiment = {};
}

/***********************************************************
* Mesh Building
************************************************************/

bool rebuildFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits)
{
	//--------------------------------------------------
	// Prepare GPU Heightmap Configuration
	//--------------------------------------------------

	GpuHeightmapNoiseConfiguration gpuHeightmap = {};

	if (world.densityField.userData ==
		&world.heightmapField)
	{
		if (!prepareFragmentCleanupGpuHeightmap(
			world.heightmapField,
			gpuHeightmap))
		{
			return false;
		}

		if (!prepareGpuHeightmapSurfaceNormals(
			experiment.normalGenerator,
			gpuHeightmap))
		{
			return false;
		}
	}

	//--------------------------------------------------
	// Clear Existing Meshes
	//--------------------------------------------------

	destroyFragmentCleanupExperimentMeshes(
		experiment);

	experiment.meshes.reserve(
		world.surfaceMap.chunks.size());

	//--------------------------------------------------
	// Build Chunk-Local CPU Meshes and Topology
	//--------------------------------------------------

	for (const DiscoveredSurfaceChunk& surfaceChunk :
		world.surfaceMap.chunks)
	{
		assert(
			surfaceChunk.chunkIndex <
			world.chunks.size());

		const Chunk& chunk =
			world.chunks[surfaceChunk.chunkIndex];

		assert(chunk.coord.x == surfaceChunk.coord.x);
		assert(chunk.coord.y == surfaceChunk.coord.y);
		assert(chunk.coord.z == surfaceChunk.coord.z);

		FragmentCleanupRenderMesh renderMesh = {};

		renderMesh.coord = surfaceChunk.coord;

		const TerrainEditChunk* editChunk =
			findTerrainEditChunk(
				edits,
				surfaceChunk.coord);

		buildEditedTetrahedraMesh(
			renderMesh.cpuMesh,
			chunk,
			surfaceChunk,
			editChunk);

		if (!buildFragmentCleanupAdjacency(
			renderMesh.cpuMesh))
		{
			std::printf(
				"Failed to build fragment cleanup adjacency.\n"
				"  Chunk: %d, %d, %d\n",
				renderMesh.coord.x,
				renderMesh.coord.y,
				renderMesh.coord.z);

			destroyFragmentCleanupExperimentMeshes(
				experiment);

			return false;
		}

		buildFragmentCleanupMaterialBoundaries(
			renderMesh.cpuMesh);

#ifndef NDEBUG
		validateFragmentCleanupAdjacency(
			renderMesh.cpuMesh);

		validateFragmentCleanupRenderMapping(
			renderMesh.cpuMesh);
#endif

		experiment.meshes.push_back(
			std::move(renderMesh));
	}

	//--------------------------------------------------
	// Build Cross-Chunk Surface Topology
	//--------------------------------------------------

	if (!buildFragmentCleanupCrossChunkConnections(
		experiment.crossChunkConnections,
		experiment.meshes))
	{
		std::printf(
			"Failed to build fragment cleanup "
			"cross-chunk connections.\n");

		destroyFragmentCleanupExperimentMeshes(
			experiment);

		return false;
	}

#ifndef NDEBUG
	validateFragmentCleanupCrossChunkConnections(
		experiment);

	updateFragmentCleanupBlendDebug(
		experiment);
#endif

	//--------------------------------------------------
	// Upload GPU Meshes and Generate Surface Normals
	//--------------------------------------------------

	for (FragmentCleanupRenderMesh& renderMesh :
		experiment.meshes)
	{
		if (!uploadFragmentCleanupRenderMesh(
			renderMesh))
		{
			destroyFragmentCleanupExperimentMeshes(
				experiment);

			return false;
		}

		if (renderMesh.gpuMesh.vertexCount == 0)
		{
			continue;
		}

		if (world.densityField.userData ==
			&world.sphereField)
		{
			if (!generateFragmentCleanupSphereNormals(
				renderMesh,
				experiment.normalGenerator,
				world.sphereField))
			{
				destroyFragmentCleanupExperimentMeshes(
					experiment);

				return false;
			}
		}
		else if (world.densityField.userData ==
			&world.heightmapField)
		{
			if (!generateFragmentCleanupHeightmapNormals(
				renderMesh,
				experiment.normalGenerator))
			{
				destroyFragmentCleanupExperimentMeshes(
					experiment);

				return false;
			}
		}
	}

	//--------------------------------------------------
	// Update Debug Statistics
	//--------------------------------------------------

	updateFragmentCleanupDebugStats(
		experiment);

	return true;
}

/***********************************************************
* Fragment Cleanup Rendering
************************************************************/

void renderFragmentCleanupExperiment(
	const FragmentCleanupExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const ChunkCoord& renderOriginChunk,
	const glm::mat4& viewProjection)
{
	StandardRenderSettings experimentRenderSettings =
		renderSettings;

	experimentRenderSettings.shadingMode =
		StandardShadingMode::TexturedLit;

	for (const FragmentCleanupRenderMesh& renderMesh :
		experiment.meshes)
	{
		if (renderMesh.gpuMesh.vertexArray == 0 ||
			renderMesh.gpuMesh.indexCount == 0)
		{
			continue;
		}

		const glm::mat4 model =
			getChunkModelMatrix(
				renderMesh.coord,
				renderOriginChunk);

		if (renderMesh.hasSmoothNormals)
		{
			renderSurfaceMesh(
				renderer,
				renderMesh.gpuMesh,
				experiment.surfaceSettings,
				experimentRenderSettings,
				terrainRenderResources.tileAtlas,
				model,
				viewProjection);
		}
		else
		{
			renderStandardMesh(
				renderer,
				renderMesh.gpuMesh,
				experimentRenderSettings,
				terrainRenderResources.tileAtlas,
				model,
				viewProjection);
		}
	}
}

/***********************************************************
* Fragment Cleanup Debug UI
************************************************************/

bool renderFragmentCleanupExperimentDebugUiContent(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world)
{
	(void)world;

	const FragmentCleanupDebugStats& stats =
		experiment.debugStats;

	ImGui::SeparatorText(
		"Fragment Cleanup Experiment");

	ImGui::TextDisabled("Geometry");

	ImGui::Text(
		"Experiment chunks: %llu",
		static_cast<unsigned long long>(
			stats.meshCount));

	ImGui::Text(
		"Mesh vertices: %llu",
		static_cast<unsigned long long>(
			stats.vertexCount));

	ImGui::Text(
		"Mesh indices: %llu",
		static_cast<unsigned long long>(
			stats.indexCount));

	ImGui::Text(
		"Surface voxel ranges: %llu",
		static_cast<unsigned long long>(
			stats.voxelRangeCount));

	ImGui::SeparatorText(
		"Surface Shading");

	ImGui::SliderFloat(
		"Normal Blend",
		&experiment.surfaceSettings.normalBlend,
		0.0f,
		1.0f,
		"%.2f");

	ImGui::Checkbox(
		"Visualize Normals",
		&experiment.surfaceSettings.visualizeNormals);

#ifndef NDEBUG

	ImGui::SeparatorText(
		"Material Blend Debug");

	FragmentCleanupBlendDebugSettings& settings =
		experiment.blendDebug;

	bool blendSettingsChanged = false;

	//--------------------------------------------------
	// Visualization Mode
	//--------------------------------------------------

	const char* debugModes[] =
	{
		"Normal",
		"Materials",
		"Boundary Seeds",
		"Blend Distance"
	};

	int selectedMode =
		static_cast<int>(settings.mode);

	if (ImGui::Combo(
		"Visualization",
		&selectedMode,
		debugModes,
		IM_ARRAYSIZE(debugModes)))
	{
		settings.mode =
			static_cast<FragmentCleanupDebugMode>(
				selectedMode);

		applyFragmentCleanupBlendDebugColors(
			experiment);
	}

	//--------------------------------------------------
	// Material Selection
	//--------------------------------------------------

	const TerrainTile materials[] =
	{
		TerrainTile::Grass,
		TerrainTile::Sand,
		TerrainTile::Rock,
		TerrainTile::Snow
	};

	if (ImGui::BeginCombo(
		"Source Material",
		getFragmentCleanupMaterialName(
			settings.sourceMaterial)))
	{
		for (TerrainTile material : materials)
		{
			if (material == settings.targetMaterial)
			{
				continue;
			}

			const bool selected =
				material == settings.sourceMaterial;

			if (ImGui::Selectable(
				getFragmentCleanupMaterialName(material),
				selected))
			{
				settings.sourceMaterial = material;
				blendSettingsChanged = true;
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo(
		"Target Material",
		getFragmentCleanupMaterialName(
			settings.targetMaterial)))
	{
		for (TerrainTile material : materials)
		{
			if (material == settings.sourceMaterial)
			{
				continue;
			}

			const bool selected =
				material == settings.targetMaterial;

			if (ImGui::Selectable(
				getFragmentCleanupMaterialName(material),
				selected))
			{
				settings.targetMaterial = material;
				blendSettingsChanged = true;
			}
		}

		ImGui::EndCombo();
	}

	//--------------------------------------------------
	// Swap Propagation Direction
	//--------------------------------------------------

	if (ImGui::Button("Swap Source / Target"))
	{
		std::swap(
			settings.sourceMaterial,
			settings.targetMaterial);

		blendSettingsChanged = true;
	}

	ImGui::Text(
		"%s -> %s",
		getFragmentCleanupMaterialName(
			settings.sourceMaterial),
		getFragmentCleanupMaterialName(
			settings.targetMaterial));

	//--------------------------------------------------
	// Relative Distance Control
	//--------------------------------------------------

	if (ImGui::SliderFloat(
		"Distance Scale",
		&settings.distanceScale,
		0.0f,
		3.0f,
		"%.2fx"))
	{
		blendSettingsChanged = true;
	}

	const float effectiveDistance =
		settings.baseDistanceMeters *
		settings.distanceScale;

	ImGui::Text(
		"Effective distance: %.2f m",
		effectiveDistance);

	if (ImGui::Button("Reset Distance Scale"))
	{
		settings.distanceScale = 1.0f;
		blendSettingsChanged = true;
	}

	//--------------------------------------------------
	// Refresh Cached Distances and GPU Colors
	//--------------------------------------------------

	if (blendSettingsChanged)
	{
		updateFragmentCleanupBlendDebug(
			experiment);
	}

#endif

	return false;
}
