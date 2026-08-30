///////////////////////////////////////////////////////////////////////////////
// renderer/render_vertex.h
// ========================
//
// Declares CPU-side vertex formats used by renderer mesh upload helpers.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>

/***********************************************************
* Render Vertex Types
************************************************************/

struct ColoredVertex
{
	glm::vec3 position = {};
	glm::vec3 color = {};
};

struct StandardVertex
{
	glm::vec3 position = {};
	glm::vec3 normal = {};
	glm::vec3 color = {};
	glm::vec2 tileUv = {};
};

// struct MaterialVertex
