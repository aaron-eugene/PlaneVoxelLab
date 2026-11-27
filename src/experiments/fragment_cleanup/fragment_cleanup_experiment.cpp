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
#include "experiments/fragment_cleanup/fragment_cleanup_cross_chunk.h"
#include "experiments/fragment_cleanup/fragment_cleanup_material_boundaries.h"
#include "experiments/fragment_cleanup/debug/fragment_cleanup_debug.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world.h"
#include "renderer/gpu_mesh.h"
#include "renderer/gpu_noise_domain.h"
#include "renderer/gpu_surface_normals.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"
#include "surface_map/surface_map.h"
#include "terrain_edit/terrain_edit.h"
#include "terrain_render/terrain_render_resources.h"

#include <glm/ext/matrix_transform.hpp>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

#if defined(PVL_ENABLE_EXPERIMENT_VALIDATION)
#include "experiments/fragment_cleanup/debug/fragment_cleanup_validation.h"
#endif

#if defined(PVL_ENABLE_EXPERIMENT_VALIDATION) && defined(NDEBUG)
#error Experiment validation requires assertions to be enabled.
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

#if defined(PVL_ENABLE_EXPERIMENT_VALIDATION)
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

#if defined(PVL_ENABLE_EXPERIMENT_VALIDATION)
	validateFragmentCleanupCrossChunkConnections(
		experiment);
#endif

#ifndef NDEBUG
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
