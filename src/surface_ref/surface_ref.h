///////////////////////////////////////////////////////////////////////////////
// surface_ref/surface_ref.h
// =========================
//
// Declares the lab's reference surface renderer.
//
// The reference surface is built from sampled chunk density data using marching
// tetrahedra. This module owns CPU-side reference meshes and the uploaded GPU
// meshes used for rendering.
//
// This module does not own chunks, density fields, the renderer, surface maps,
// or lab-world data.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "chunk/chunk.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/gpu_mesh.h"
#include "surface/surface_map.h"
#include "surface_ref/marching_tetrahedra/tetrahedra_builder.h"

#include <glm/mat4x4.hpp>

#include <vector>

struct Renderer;

/***********************************************************
* Surface Reference Types
************************************************************/

struct SurfaceRefChunk
{
	ChunkCoord coord = {};

	TetrahedraMesh cpuMesh = {};
	GpuMesh gpuMesh = {};
};

struct SurfaceRef
{
	std::vector<SurfaceRefChunk> chunks = {};
};

/***********************************************************
* Surface Reference Lifetime
************************************************************/

bool initializeSurfaceRef(
	SurfaceRef& surfaceRef,
	const std::vector<Chunk>& chunks,
	const SurfaceMap& surfaceMap);

void shutdownSurfaceRef(
	SurfaceRef& surfaceRef);

/***********************************************************
* Surface Reference Mesh Rebuild
************************************************************/

bool rebuildSurfaceRef(
	SurfaceRef& surfaceRef,
	const std::vector<Chunk>& chunks,
	const SurfaceMap& surfaceMap);

/***********************************************************
* Surface Reference Rendering
************************************************************/

void renderSurfaceRef(
	const SurfaceRef& surfaceRef,
	Renderer& renderer,
	const glm::mat4& viewProjection);
