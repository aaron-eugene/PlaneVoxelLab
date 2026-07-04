///////////////////////////////////////////////////////////////////////////////
// renderer/render_vertex.h
// ========================
//
// Declares CPU-side vertex formats used by renderer mesh upload helpers.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/vec3.hpp>

/***********************************************************
* Colored Vertex
************************************************************/

struct ColoredVertex
{
	glm::vec3 position = {};
	glm::vec3 color = {};
};
