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

#include <cstddef>
#include <type_traits>

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

/***********************************************************
* Standard Vertex Layout Validation
************************************************************/

static_assert(std::is_standard_layout_v<StandardVertex>);

static_assert(sizeof(StandardVertex) == 44);

static_assert(offsetof(StandardVertex, position) == 0);
static_assert(offsetof(StandardVertex, normal) == 12);
static_assert(offsetof(StandardVertex, color) == 24);
static_assert(offsetof(StandardVertex, tileUv) == 36);

