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
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
			std::min<size_t>(
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
* Fragment Cleanup Shader Helpers
************************************************************/

static void shutdownFragmentCleanupShader(
	FragmentCleanupShader& shader)
{
	destroyShaderProgram(
		shader.program);

	shader = {};
}

static bool initializeFragmentCleanupShader(
	FragmentCleanupShader& shader)
{
	assert(shader.program.handle == 0);

	ShaderSource source = {};

	if (!loadShaderSource(
		source,
		"assets/shaders/fragment_cleanup.vert",
		"assets/shaders/fragment_cleanup.frag"))
	{
		return false;
	}

	const bool created =
		createShaderProgram(
			shader.program,
			source);

	clearShaderSource(source);

	if (!created)
	{
		return false;
	}

	shader.modelLocation =
		getShaderUniformLocation(
			shader.program,
			"uModel");

	shader.viewProjectionLocation =
		getShaderUniformLocation(
			shader.program,
			"uViewProjection");

	shader.textureLocation =
		getShaderUniformLocation(
			shader.program,
			"uTexture");

	shader.lightDirectionLocation =
		getShaderUniformLocation(
			shader.program,
			"uLightDirection");

	shader.ambientStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			"uAmbientStrength");

	shader.diffuseStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			"uDiffuseStrength");

	shader.normalBlendLocation =
		getShaderUniformLocation(
			shader.program,
			"uNormalBlend");

	shader.visualizeNormalsLocation =
		getShaderUniformLocation(
			shader.program,
			"uVisualizeNormals");

	if (shader.modelLocation < 0 ||
		shader.viewProjectionLocation < 0 ||
		shader.textureLocation < 0 ||
		shader.lightDirectionLocation < 0 ||
		shader.ambientStrengthLocation < 0 ||
		shader.diffuseStrengthLocation < 0 ||
		shader.normalBlendLocation < 0 ||
		shader.visualizeNormalsLocation < 0)
	{
		shutdownFragmentCleanupShader(
			shader);

		return false;
	}

	return true;
}

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

	if (!createStandardGpuMesh(
		renderMesh.gpuMesh,
		renderMesh.cpuMesh.vertices.data(),
		vertexCount,
		renderMesh.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles))
	{
		return false;
	}

	if (!createGpuSurfaceNormalBuffer(
		renderMesh.smoothNormals,
		vertexCount))
	{
		destroyFragmentCleanupRenderMesh(
			renderMesh);

		return false;
	}

	attachGpuMeshSmoothNormals(
		renderMesh.gpuMesh,
		renderMesh.smoothNormals.handle);

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
* Fragment Cleanup Drawing Helpers
************************************************************/

static void renderFragmentCleanupMesh(
	const FragmentCleanupShader& shader,
	const GpuMesh& mesh,
	const StandardRenderSettings& settings,
	const Texture2D& texture,
	float normalBlend,
	bool visualizeNormals,
	const glm::mat4& model,
	const glm::mat4& viewProjection)
{
	assert(shader.program.handle != 0);

	assert(mesh.vertexArray != 0);
	assert(mesh.indexCount > 0);
	assert(mesh.primitiveType == GpuPrimitiveType::Triangles);

	assert(texture.handle != 0);

	assert(settings.ambientStrength >= 0.0f);
	assert(settings.diffuseStrength >= 0.0f);

	assert(normalBlend >= 0.0f);
	assert(normalBlend <= 1.0f);

	constexpr GLuint textureUnit = 0;

	glUseProgram(
		shader.program.handle);

	glUniformMatrix4fv(
		shader.modelLocation,
		1,
		GL_FALSE,
		glm::value_ptr(model));

	glUniformMatrix4fv(
		shader.viewProjectionLocation,
		1,
		GL_FALSE,
		glm::value_ptr(viewProjection));

	glUniform3fv(
		shader.lightDirectionLocation,
		1,
		glm::value_ptr(
			settings.lightDirection));

	glUniform1f(
		shader.ambientStrengthLocation,
		settings.ambientStrength);

	glUniform1f(
		shader.diffuseStrengthLocation,
		settings.diffuseStrength);

	glUniform1f(
		shader.normalBlendLocation,
		normalBlend);

	glUniform1i(
		shader.visualizeNormalsLocation,
		visualizeNormals ? 1 : 0);

	glBindTextureUnit(
		textureUnit,
		texture.handle);

	glUniform1i(
		shader.textureLocation,
		static_cast<GLint>(
			textureUnit));

	glBindVertexArray(
		mesh.vertexArray);

	glDrawElements(
		GL_TRIANGLES,
		mesh.indexCount,
		GL_UNSIGNED_INT,
		nullptr);

	glBindVertexArray(0);

	glBindTextureUnit(
		textureUnit,
		0);

	glUseProgram(0);
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
	assert(experiment.shader.program.handle == 0);

	experiment = {};

	if (!initializeGpuSurfaceNormalGenerator(
		experiment.normalGenerator))
	{
		shutdownFragmentCleanupExperiment(
			experiment);

		return false;
	}

	if (!initializeFragmentCleanupShader(
		experiment.shader))
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

	shutdownFragmentCleanupShader(
		experiment.shader);

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

		// Sphere-only validation path.
		// Other fields will use their own GPU gradient implementation.
		if (world.densityField.userData ==
			&world.sphereField &&
			renderMesh.smoothNormals.handle != 0)
		{
			generateGpuSphereSurfaceNormals(
				experiment.normalGenerator,
				renderMesh.gpuMesh,
				renderMesh.smoothNormals,
				renderMesh.coord,
				world.sphereField);

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

			renderMesh.hasSmoothNormals = true;
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
			renderFragmentCleanupMesh(
				experiment.shader,
				renderMesh.gpuMesh,
				experimentRenderSettings,
				terrainRenderResources.tileAtlas,
				experiment.normalBlend,
				experiment.visualizeNormals,
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
		&experiment.normalBlend,
		0.0f,
		1.0f,
		"%.2f");

	ImGui::Checkbox(
		"Visualize Normals",
		&experiment.visualizeNormals);

	return false;
}
