///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.cpp
// ============================================================
//
// Implements the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"

#include "chunk/chunk.h"
#include "fields/field_generators.h"
#include "lab_world/lab_world.h"
#include "renderer/gpu_mesh.h"
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
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>

/***********************************************************
* GPU Surface Normal Validation
************************************************************/

#ifndef NDEBUG

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

#ifndef NDEBUG
	validateGpuMeshSmoothNormalAttribute(
		renderMesh.gpuMesh,
		renderMesh.smoothNormals.handle);
#endif

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
	assert(experiment.normalGenerator.program.handle == 0);

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

		if (!uploadFragmentCleanupRenderMesh(
			renderMesh))
		{
			destroyFragmentCleanupRenderMesh(
				renderMesh);

			destroyFragmentCleanupExperimentMeshes(
				experiment);

			return false;
		}

		// Generate continuous normals only for supported fields.
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
