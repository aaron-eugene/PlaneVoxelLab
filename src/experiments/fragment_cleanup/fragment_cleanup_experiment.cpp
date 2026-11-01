///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.cpp
// ============================================================
//
// Implements the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"

#include "chunk/chunk.h"
#include "lab_world/lab_world.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"
#include "surface_map/surface_map.h"
#include "terrain_edit/terrain_edit.h"
#include "terrain_render/terrain_render_resources.h"

#include <imgui/imgui.h>

#include <glm/ext/matrix_transform.hpp>

#include <cassert>
#include <cstdint>
#include <utility>

/***********************************************************
* Render-Mesh Helpers
************************************************************/

static void destroyFragmentCleanupRenderMesh(
	FragmentCleanupRenderMesh& renderMesh)
{
	destroyGpuMesh(renderMesh.gpuMesh);

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

	assert(
		renderMesh.cpuMesh.vertices.empty() ==
		renderMesh.cpuMesh.indices.empty());

	if (renderMesh.cpuMesh.vertices.empty())
	{
		return true;
	}

	const uint32_t vertexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.vertices.size());

	const uint32_t indexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.indices.size());

	return createColoredGpuMesh(
		renderMesh.gpuMesh,
		renderMesh.cpuMesh.vertices.data(),
		vertexCount,
		renderMesh.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles);
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
	const SurfaceChunk& surfaceChunk,
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
* Fragment Cleanup Lifecycle
************************************************************/

bool initializeFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits)
{
	assert(experiment.meshes.empty());

	experiment = {};

	return rebuildFragmentCleanupExperiment(
		experiment,
		world,
		edits);
}

void shutdownFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment)
{
	destroyFragmentCleanupExperimentMeshes(
		experiment);

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
	destroyFragmentCleanupExperimentMeshes(
		experiment);

	experiment.meshes.reserve(
		world.surfaceMap.chunks.size());

	for (const SurfaceChunk& surfaceChunk :
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

		if (!uploadFragmentCleanupRenderMesh(
			renderMesh))
		{
			destroyFragmentCleanupRenderMesh(
				renderMesh);

			destroyFragmentCleanupExperimentMeshes(
				experiment);

			return false;
		}

		experiment.meshes.push_back(
			std::move(renderMesh));
	}

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
	(void)renderSettings;
	(void)terrainRenderResources;

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

		renderColoredMesh(
			renderer,
			renderMesh.gpuMesh,
			model,
			viewProjection);
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

	return false;
}