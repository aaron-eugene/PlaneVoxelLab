///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_clipping.h
// ================================================
//
// Declares temporary polygon types and Y-slab clipping helpers used during
// XZ columnar mesh construction.
//
// This module clips construction polygons against horizontal Y boundaries.
// It does not determine ownership, construct planar cells, emit mesh data,
// or own persistent resources.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

/***********************************************************
* XZ Columnar Clipping Constants
************************************************************/

constexpr float XZ_COLUMNAR_CLIPPING_EPSILON =
	0.00001f;

constexpr uint32_t
	MAX_XZ_COLUMNAR_CLIPPED_POLYGON_VERTICES = 8;

/***********************************************************
* XZ Columnar Clipping Types
************************************************************/

struct XZColumnarClipVertex
{
	glm::vec3 position = {};
	glm::vec3 color = {};
};

struct XZColumnarClipPolygon
{
	XZColumnarClipVertex vertices[
		MAX_XZ_COLUMNAR_CLIPPED_POLYGON_VERTICES] = {};

	uint32_t vertexCount = 0;
};

/***********************************************************
* XZ Columnar Clipping Interface
************************************************************/

void appendXZColumnarClipVertex(
	XZColumnarClipPolygon& polygon,
	const XZColumnarClipVertex& vertex);

void removeClosingDuplicateXZColumnarClipVertex(
	XZColumnarClipPolygon& polygon);

XZColumnarClipPolygon clipXZColumnarPolygonToYSlab(
	const XZColumnarClipPolygon& polygon,
	float minY,
	float maxY);

float getXZColumnarPolygonMinY(
	const XZColumnarClipPolygon& polygon);

float getXZColumnarPolygonMaxY(
	const XZColumnarClipPolygon& polygon);

glm::vec3 getXZColumnarClipPolygonNormal(
	const XZColumnarClipPolygon& polygon);

void setXZColumnarClipPolygonColor(
	XZColumnarClipPolygon& polygon,
	const glm::vec3& color);
