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
#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"
#include "experiments/fragment_cleanup/fragment_cleanup_edge_diagnostics.h"
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
* GPU Surface Normal Validation
************************************************************/

static void validateFragmentCleanupSphereNormals(
	const FragmentCleanupRenderMesh& renderMesh,
	const SphereDensityField& sphere)
{
	assert(renderMesh.smoothNormals.handle != 0);
	assert(!renderMesh.cpuMesh.vertices.empty());

	const glm::vec3 sphereCenterLocal =
		getChunkRelativePosition(
			sphere.center.chunk,
			renderMesh.coord) +
		sphere.center.localPosition;

	const uint32_t sampleCount =
		static_cast<uint32_t>(
			std::min<std::size_t>(
				8,
				renderMesh.cpuMesh.vertices.size()));

	for (uint32_t index = 0;
		index < sampleCount;
		++index)
	{
		glm::vec4 gpuNormal = {};

		glGetNamedBufferSubData(
			renderMesh.smoothNormals.handle,
			static_cast<GLintptr>(
				index * sizeof(glm::vec4)),
			sizeof(glm::vec4),
			&gpuNormal);

		const glm::vec3 position =
			renderMesh.cpuMesh.vertices[index].position;

		const glm::vec3 direction =
			position - sphereCenterLocal;

		const float lengthSquared =
			glm::dot(direction, direction);

		if (lengthSquared <= 0.000001f)
		{
			continue;
		}

		const glm::vec3 expectedNormal =
			glm::normalize(direction);

		const glm::vec3 actualNormal =
			glm::vec3(gpuNormal);

		const float error =
			glm::length(
				actualNormal - expectedNormal);

		assert(error < 0.001f);
	}
}

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
* GPU Heightmap Normal Validation
************************************************************/

static void validateFragmentCleanupHeightmapNormals(
	const FragmentCleanupRenderMesh& renderMesh,
	const HeightmapDensityField& heightmap)
{
	assert(renderMesh.hasSmoothNormals);
	assert(renderMesh.smoothNormals.handle != 0);
	assert(renderMesh.smoothNormals.normalCount ==
		renderMesh.cpuMesh.vertices.size());

	const uint32_t normalCount =
		renderMesh.smoothNormals.normalCount;

	assert(normalCount > 0);

	std::vector<glm::vec4> gpuNormals(normalCount);

	glGetNamedBufferSubData(
		renderMesh.smoothNormals.handle,
		0,
		static_cast<GLsizeiptr>(
			gpuNormals.size() * sizeof(glm::vec4)),
		gpuNormals.data());

	// Inspect every normal in this buffer for invalid output.
	for (uint32_t index = 0; index < normalCount; ++index)
	{
		const glm::vec4& normal = gpuNormals[index];

		const bool finite =
			std::isfinite(normal.x) &&
			std::isfinite(normal.y) &&
			std::isfinite(normal.z) &&
			std::isfinite(normal.w);

		const float lengthSquared =
			normal.x * normal.x +
			normal.y * normal.y +
			normal.z * normal.z;

		if (!finite ||
			normal.w != 0.0f ||
			!std::isfinite(lengthSquared) ||
			std::fabs(lengthSquared - 1.0f) > 0.01f)
		{
			std::printf(
				"Invalid GPU heightmap normal.\n"
				"  Chunk: %d, %d, %d\n"
				"  Vertex: %u\n"
				"  Normal: %.6f %.6f %.6f %.6f\n",
				renderMesh.coord.x,
				renderMesh.coord.y,
				renderMesh.coord.z,
				index,
				normal.x,
				normal.y,
				normal.z,
				normal.w);

			assert(false &&
				"Invalid GPU heightmap normal");

			return;
		}
	}

	// Compare representative vertices against the CPU reference.
	const uint32_t sampleCount =
		std::min<uint32_t>(8u, normalCount);

	for (uint32_t sample = 0;
		sample < sampleCount;
		++sample)
	{
		const uint32_t index =
			sampleCount > 1
			? static_cast<uint32_t>(
				(static_cast<uint64_t>(sample) *
					(normalCount - 1u)) /
				(sampleCount - 1u))
			: 0u;

		const glm::vec3 localPosition =
			renderMesh.cpuMesh.vertices[index].position;

		const WorldPosition worldPosition =
			makeWorldPosition(
				renderMesh.coord,
				localPosition);

		const glm::vec3 expected =
			sampleFragmentCleanupCpuHeightmapNormal(
				worldPosition,
				heightmap);

		const glm::vec3 actual =
			glm::vec3(gpuNormals[index]);

		const float error =
			glm::length(actual - expected);

		if (!std::isfinite(error) ||
			error > HEIGHTMAP_NORMAL_TOLERANCE)
		{
			std::printf(
				"GPU heightmap normal mismatch.\n"
				"  Chunk: %d, %d, %d\n"
				"  Vertex: %u\n"
				"  Error: %.9f\n",
				renderMesh.coord.x,
				renderMesh.coord.y,
				renderMesh.coord.z,
				index,
				error);

			assert(false &&
				"GPU heightmap normal mismatch");

			return;
		}
	}
}

/***********************************************************
* Triangle Adjacency Validation
************************************************************/

static void validateFragmentCleanupAdjacency(
	const FragmentCleanupMesh& mesh)
{
	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());

	uint64_t matchedEdgeOccurrences = 0;
	uint64_t unmatchedEdgeOccurrences = 0;
	uint64_t renderedRejectedConnections = 0;

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.adjacency.size();
		++triangleIndex)
	{
		const FragmentCleanupTriangleAdjacency& triangle =
			mesh.adjacency[triangleIndex];

		for (uint32_t edgeIndex = 0;
			edgeIndex < 3;
			++edgeIndex)
		{
			const uint32_t neighborIndex =
				triangle.neighbors[edgeIndex];

			if (neighborIndex == UINT32_MAX)
			{
				++unmatchedEdgeOccurrences;
				continue;
			}

			assert(neighborIndex < mesh.adjacency.size());
			assert(neighborIndex != triangleIndex);

			++matchedEdgeOccurrences;

			const FragmentCleanupTriangleAdjacency& neighbor =
				mesh.adjacency[neighborIndex];

			bool reciprocal = false;

			for (uint32_t neighborEdgeIndex = 0;
				neighborEdgeIndex < 3;
				++neighborEdgeIndex)
			{
				if (neighbor.neighbors[neighborEdgeIndex] ==
					triangleIndex)
				{
					reciprocal = true;
					break;
				}
			}

			assert(reciprocal);

			if (triangleIndex < neighborIndex)
			{
				const bool renderedA =
					mesh.topologyTriangles[triangleIndex]
					.renderTriangleIndex != UINT32_MAX;

				const bool renderedB =
					mesh.topologyTriangles[neighborIndex]
					.renderTriangleIndex != UINT32_MAX;

				if (renderedA != renderedB)
				{
					++renderedRejectedConnections;
				}
			}
		}
	}

	assert(matchedEdgeOccurrences % 2 == 0);

	assert(
		matchedEdgeOccurrences +
		unmatchedEdgeOccurrences ==
		mesh.topologyTriangles.size() * 3);

	std::printf(
		"Fragment cleanup adjacency validation:\n"
		"  Topology triangles:         %zu\n"
		"  Matched edge pairs:         %llu\n"
		"  Unmatched edge occurrences: %llu\n"
		"  Rendered/rejected links:    %llu\n",
		mesh.topologyTriangles.size(),
		static_cast<unsigned long long>(
			matchedEdgeOccurrences / 2),
		static_cast<unsigned long long>(
			unmatchedEdgeOccurrences),
		static_cast<unsigned long long>(
			renderedRejectedConnections));
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
	for (FragmentCleanupRenderMesh& renderMesh :
		experiment.meshes)
	{
		destroyFragmentCleanupRenderMesh(
			renderMesh);
	}

	experiment.meshes.clear();
	experiment.debugStats = {};
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

/***********************************************************
* Mesh Building Helpers
************************************************************/

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

	assert(
		mesh.triangleKeys.size() ==
		mesh.indices.size() / 3);

	assert(
		mesh.topologyTriangles.size() >=
		mesh.triangleKeys.size());
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
#ifndef NDEBUG
	bool sphereNormalsValidated = false;
#endif

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

	destroyFragmentCleanupExperimentMeshes(
		experiment);

	experiment.meshes.reserve(
		world.surfaceMap.chunks.size());

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

		if (!buildFragmentCleanupAdjacency(renderMesh.cpuMesh))
		{
			std::printf(
				"Failed to build fragment cleanup adjacency.\n"
				"  Chunk: %d, %d, %d\n",
				renderMesh.coord.x,
				renderMesh.coord.y,
				renderMesh.coord.z);

			destroyFragmentCleanupRenderMesh(renderMesh);
			destroyFragmentCleanupExperimentMeshes(experiment);

			return false;
		}

#ifndef NDEBUG
		static bool adjacencyValidated = false;

		if (!adjacencyValidated &&
			!renderMesh.cpuMesh.topologyTriangles.empty())
		{
			validateFragmentCleanupAdjacency(
				renderMesh.cpuMesh);

			adjacencyValidated = true;
		}
#endif

#ifndef NDEBUG
		static bool edgeDiagnosticReported = false;

		if (!edgeDiagnosticReported &&
			!renderMesh.cpuMesh.triangleKeys.empty())
		{
			assert(
				renderMesh.cpuMesh.triangleKeys.size() ==
				renderMesh.cpuMesh.indices.size() / 3);

			const FragmentCleanupEdgeStats stats =
				analyzeFragmentCleanupEdges(
					renderMesh.cpuMesh);

			std::printf(
				"Fragment cleanup edge diagnostic:\n"
				"  Chunk: %d, %d, %d\n"
				"  Triangles:        %llu\n"
				"  Rendered triangles: %llu\n"
				"  Rejected triangles: %llu\n"
				"  Edge occurrences: %llu\n"
				"  Unique edges:     %llu\n"
				"  Single-use:       %llu\n"
				"    Chunk boundary: %llu\n"
				"    Interior:       %llu\n"
				"  Double-use:       %llu\n"
				"  Multi-use:        %llu\n"
				"  Maximum use:      %u\n"
				"  Analysis time:    %.3f ms\n",
				renderMesh.coord.x,
				renderMesh.coord.y,
				renderMesh.coord.z,
				static_cast<unsigned long long>(stats.triangleCount),
				static_cast<unsigned long long>(stats.renderedTriangleCount),
				static_cast<unsigned long long>(stats.rejectedTriangleCount),
				static_cast<unsigned long long>(stats.edgeOccurrences),
				static_cast<unsigned long long>(stats.uniqueEdges),
				static_cast<unsigned long long>(stats.singleUseEdges),
				static_cast<unsigned long long>(stats.boundarySingleUseEdges),
				static_cast<unsigned long long>(stats.interiorSingleUseEdges),
				static_cast<unsigned long long>(stats.doubleUseEdges),
				static_cast<unsigned long long>(stats.multiUseEdges),
				stats.maximumEdgeUse,
				stats.elapsedMilliseconds);

			edgeDiagnosticReported = true;
		}
#endif

		if (!uploadFragmentCleanupRenderMesh(
			renderMesh))
		{
			destroyFragmentCleanupRenderMesh(
				renderMesh);

			destroyFragmentCleanupExperimentMeshes(
				experiment);

			return false;
		}

		if (world.densityField.userData ==
			&world.sphereField &&
			renderMesh.gpuMesh.vertexCount > 0)
		{
			if (!generateFragmentCleanupSphereNormals(
				renderMesh,
				experiment.normalGenerator,
				world.sphereField))
			{
				destroyFragmentCleanupRenderMesh(
					renderMesh);

				destroyFragmentCleanupExperimentMeshes(
					experiment);

				return false;
			}

#ifndef NDEBUG
			if (!sphereNormalsValidated)
			{
				validateFragmentCleanupSphereNormals(
					renderMesh,
					world.sphereField);

				sphereNormalsValidated = true;

				std::printf(
					"GPU sphere-normal validation passed.\n");
			}
#endif
		}

		else if (world.densityField.userData ==
			&world.heightmapField &&
			renderMesh.gpuMesh.vertexCount > 0)
		{
			if (!generateFragmentCleanupHeightmapNormals(
				renderMesh,
				experiment.normalGenerator))
			{
				destroyFragmentCleanupRenderMesh(renderMesh);
				destroyFragmentCleanupExperimentMeshes(experiment);

				return false;
			}
		}

		experiment.meshes.push_back(
			std::move(renderMesh));
	}


#ifndef NDEBUG

	if (world.densityField.userData ==
		&world.heightmapField)
	{
		std::vector<std::size_t> meshIndices = {};

		for (std::size_t index = 0;
			index < experiment.meshes.size();
			++index)
		{
			if (experiment.meshes[index].hasSmoothNormals)
			{
				meshIndices.push_back(index);
			}
		}

		if (!meshIndices.empty())
		{
			const std::size_t selectedIndices[] =
			{
				0,
				meshIndices.size() / 2,
				meshIndices.size() - 1
			};

			std::size_t validatedMeshCount = 0;
			std::size_t validatedNormalCount = 0;

			std::size_t previousIndex =
				std::numeric_limits<std::size_t>::max();

			for (std::size_t selectedIndex : selectedIndices)
			{
				const std::size_t meshIndex =
					meshIndices[selectedIndex];

				if (meshIndex == previousIndex)
				{
					continue;
				}

				const FragmentCleanupRenderMesh& renderMesh =
					experiment.meshes[meshIndex];

				validateFragmentCleanupHeightmapNormals(
					renderMesh,
					world.heightmapField);

				++validatedMeshCount;

				validatedNormalCount +=
					renderMesh.smoothNormals.normalCount;

				previousIndex = meshIndex;
			}

			std::printf(
				"GPU heightmap production validation passed.\n"
				"  Meshes: %zu\n"
				"  Normals checked: %zu\n",
				validatedMeshCount,
				validatedNormalCount);
		}
	}

#endif

	updateFragmentCleanupDebugStats(experiment);

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

	return false;
}
