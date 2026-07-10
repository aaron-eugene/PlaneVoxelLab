///////////////////////////////////////////////////////////////////////////////
// surface_ref/marching_tetrahedra/tetrahedra_builder.h
// ====================================================
//
// Declares helpers for building a reference surface mesh from chunk density
// samples using marching tetrahedra.
//
// This module reads chunk density samples and emits a CPU-side colored triangle
// mesh. It does not own chunks, density fields, or GPU resources.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/render_vertex.h"

#include <cstdint>
#include <vector>

struct Chunk;
struct SurfaceChunk;

/***********************************************************
* Marching Tetrahedra Mesh Types
************************************************************/

struct TetrahedraMesh
{
	std::vector<ColoredVertex> vertices = {};
	std::vector<uint32_t> indices = {};
};

/***********************************************************
* Marching Tetrahedra Builder Interface
************************************************************/

void clearTetrahedraMesh(
	TetrahedraMesh& mesh);

bool buildTetrahedraMesh(
	TetrahedraMesh& mesh,
	const Chunk& chunk,
	const SurfaceChunk& surfaceChunk);
