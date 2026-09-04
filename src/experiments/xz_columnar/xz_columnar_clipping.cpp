///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_clipping.cpp
// ================================================
//
// Implements temporary polygon construction and horizontal Y-slab clipping
// used by the XZ columnar mesh builder.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_clipping.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

/***********************************************************
* Local Clipping Helpers
************************************************************/

static bool clipVertexPositionsNearlyEqual(
	const XZColumnarClipVertex& a,
	const XZColumnarClipVertex& b)
{
	return
		std::abs(a.position.x - b.position.x) <=
		XZ_COLUMNAR_CLIPPING_EPSILON &&
		std::abs(a.position.y - b.position.y) <=
		XZ_COLUMNAR_CLIPPING_EPSILON &&
		std::abs(a.position.z - b.position.z) <=
		XZ_COLUMNAR_CLIPPING_EPSILON;
}

static XZColumnarClipVertex interpolateClipVertexAtY(
	const XZColumnarClipVertex& a,
	const XZColumnarClipVertex& b,
	float planeY)
{
	const float deltaY =
		b.position.y - a.position.y;

	assert(std::abs(deltaY) > XZ_COLUMNAR_CLIPPING_EPSILON);

	const float t =
		(planeY - a.position.y) / deltaY;

	XZColumnarClipVertex result = {};

	result.position =
		a.position +
		(b.position - a.position) * t;

	result.color =
		a.color +
		(b.color - a.color) * t;

	return result;
}

static void clipPolygonMinY(
	const XZColumnarClipPolygon& input,
	XZColumnarClipPolygon& output,
	float minY)
{
	output = {};

	if (input.vertexCount == 0)
	{
		return;
	}

	for (uint32_t vertexIndex = 0;
		vertexIndex < input.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& current =
			input.vertices[vertexIndex];

		const XZColumnarClipVertex& previous =
			input.vertices[
				(vertexIndex + input.vertexCount - 1) %
					input.vertexCount];

		const bool currentInside =
			current.position.y >= minY - XZ_COLUMNAR_CLIPPING_EPSILON;

		const bool previousInside =
			previous.position.y >= minY - XZ_COLUMNAR_CLIPPING_EPSILON;

		if (currentInside != previousInside)
		{
			appendXZColumnarClipVertex(
				output,
				interpolateClipVertexAtY(
					previous,
					current,
					minY));
		}

		if (currentInside)
		{
			appendXZColumnarClipVertex(
				output,
				current);
		}
	}
}

static void clipPolygonMaxY(
	const XZColumnarClipPolygon& input,
	XZColumnarClipPolygon& output,
	float maxY)
{
	output = {};

	if (input.vertexCount == 0)
	{
		return;
	}

	for (uint32_t vertexIndex = 0;
		vertexIndex < input.vertexCount;
		++vertexIndex)
	{
		const XZColumnarClipVertex& current =
			input.vertices[vertexIndex];

		const XZColumnarClipVertex& previous =
			input.vertices[
				(vertexIndex + input.vertexCount - 1) %
					input.vertexCount];

		const bool currentInside =
			current.position.y <=
			maxY + XZ_COLUMNAR_CLIPPING_EPSILON;

		const bool previousInside =
			previous.position.y <=
			maxY + XZ_COLUMNAR_CLIPPING_EPSILON;

		if (currentInside != previousInside)
		{
			appendXZColumnarClipVertex(
				output,
				interpolateClipVertexAtY(
					previous,
					current,
					maxY));
		}

		if (currentInside)
		{
			appendXZColumnarClipVertex(
				output,
				current);
		}
	}
}

static bool findXZColumnarClipPolygonCrossProduct(
	glm::vec3& crossProduct,
	const XZColumnarClipPolygon& polygon)
{
	if (polygon.vertexCount < 3)
	{
		crossProduct = {};
		return false;
	}

	for (uint32_t vertexIndex = 1;
		vertexIndex + 1 < polygon.vertexCount;
		++vertexIndex)
	{
		const glm::vec3 edgeA =
			polygon.vertices[vertexIndex].position -
			polygon.vertices[0].position;

		const glm::vec3 edgeB =
			polygon.vertices[vertexIndex + 1].position -
			polygon.vertices[0].position;

		const glm::vec3 candidateCrossProduct =
			glm::cross(
				edgeA,
				edgeB);

		const float lengthSquared =
			glm::dot(
				candidateCrossProduct,
				candidateCrossProduct);

		if (lengthSquared >
			XZ_COLUMNAR_CLIPPING_EPSILON *
			XZ_COLUMNAR_CLIPPING_EPSILON)
		{
			crossProduct =
				candidateCrossProduct;

			return true;
		}
	}

	crossProduct = {};
	return false;
}

/***********************************************************
* XZ Columnar Clipping Interface
************************************************************/

bool hasXZColumnarClipPolygonArea(
	const XZColumnarClipPolygon& polygon)
{
	glm::vec3 crossProduct = {};

	return findXZColumnarClipPolygonCrossProduct(
		crossProduct,
		polygon);
}

void appendXZColumnarClipVertex(
	XZColumnarClipPolygon& polygon,
	const XZColumnarClipVertex& vertex)
{
	if (polygon.vertexCount > 0)
	{
		const XZColumnarClipVertex& previous =
			polygon.vertices[
				polygon.vertexCount - 1];

		if (clipVertexPositionsNearlyEqual(
			previous,
			vertex))
		{
			return;
		}
	}

	assert(
		polygon.vertexCount <
		MAX_XZ_COLUMNAR_CLIPPED_POLYGON_VERTICES);

	polygon.vertices[polygon.vertexCount] =
		vertex;

	++polygon.vertexCount;
}

void removeClosingDuplicateXZColumnarClipVertex(
	XZColumnarClipPolygon& polygon)
{
	if (polygon.vertexCount < 2)
	{
		return;
	}

	if (clipVertexPositionsNearlyEqual(
		polygon.vertices[0],
		polygon.vertices[
			polygon.vertexCount - 1]))
	{
		--polygon.vertexCount;
	}
}

XZColumnarClipPolygon clipXZColumnarPolygonToYSlab(
	const XZColumnarClipPolygon& polygon,
	float minY,
	float maxY)
{
	assert(maxY > minY);

	XZColumnarClipPolygon clippedMin = {};
	XZColumnarClipPolygon clippedMax = {};

	clipPolygonMinY(
		polygon,
		clippedMin,
		minY);

	clipPolygonMaxY(
		clippedMin,
		clippedMax,
		maxY);

	return clippedMax;
}

float getXZColumnarPolygonMinY(
	const XZColumnarClipPolygon& polygon)
{
	assert(polygon.vertexCount > 0);

	float minY =
		polygon.vertices[0].position.y;

	for (uint32_t vertexIndex = 1;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		minY =
			std::min(
				minY,
				polygon.vertices[vertexIndex].position.y);
	}

	return minY;
}

float getXZColumnarPolygonMaxY(
	const XZColumnarClipPolygon& polygon)
{
	assert(polygon.vertexCount > 0);

	float maxY =
		polygon.vertices[0].position.y;

	for (uint32_t vertexIndex = 1;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		maxY =
			std::max(
				maxY,
				polygon.vertices[vertexIndex].position.y);
	}

	return maxY;
}

bool calculateXZColumnarClipPolygonNormal(
	glm::vec3& normal,
	const XZColumnarClipPolygon& polygon)
{
	glm::vec3 crossProduct = {};

	if (!findXZColumnarClipPolygonCrossProduct(
		crossProduct,
		polygon))
	{
		normal = {};
		return false;
	}

	normal =
		glm::normalize(
			crossProduct);

	return true;
}

void setXZColumnarClipPolygonColor(
	XZColumnarClipPolygon& polygon,
	const glm::vec3& color)
{
	for (uint32_t vertexIndex = 0;
		vertexIndex < polygon.vertexCount;
		++vertexIndex)
	{
		polygon.vertices[vertexIndex].color =
			color;
	}
}
