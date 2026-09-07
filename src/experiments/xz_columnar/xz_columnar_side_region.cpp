///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_side_region.cpp
// ===================================================
//
// Implements exposed side-region construction for neighboring XZ columnar
// planar cells.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_side_region.h"

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "experiments/xz_columnar/xz_columnar_planar_cell.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>

/***********************************************************
* Side Region Constants
************************************************************/

static constexpr float XZ_COLUMNAR_SIDE_EPSILON = 0.00001f;

/***********************************************************
* Side Direction Helpers
************************************************************/

static bool areOpposingColumnarSides(
	XZColumnarSide sideA,
	XZColumnarSide sideB)
{
	return
		(sideA == XZColumnarSide::NegativeX &&
			sideB == XZColumnarSide::PositiveX) ||
		(sideA == XZColumnarSide::PositiveX &&
			sideB == XZColumnarSide::NegativeX) ||
		(sideA == XZColumnarSide::NegativeZ &&
			sideB == XZColumnarSide::PositiveZ) ||
		(sideA == XZColumnarSide::PositiveZ &&
			sideB == XZColumnarSide::NegativeZ);
}

/***********************************************************
* Side Region Construction Helpers
************************************************************/

static glm::vec3 interpolateEdgeProfile(
	const XZColumnarEdgeProfile& profile,
	float t)
{
	assert(t >= 0.0f);
	assert(t <= 1.0f);

	return profile.start +
		(profile.end - profile.start) * t;
}

static XZColumnarSideRegion buildSideRegion(
	const XZColumnarPlanarCell& cellA,
	const XZColumnarEdgeProfile& profileA,
	XZColumnarSide sideA,
	const XZColumnarPlanarCell& cellB,
	const XZColumnarEdgeProfile& profileB,
	XZColumnarSide sideB,
	float startT,
	float endT)
{
	assert(startT >= 0.0f);
	assert(endT <= 1.0f);
	assert(endT > startT);

	const float sampleT =
		(startT + endT) * 0.5f;

	const glm::vec3 sampleA =
		interpolateEdgeProfile(
			profileA,
			sampleT);

	const glm::vec3 sampleB =
		interpolateEdgeProfile(
			profileB,
			sampleT);

	XZColumnarSideRegion region = {};

	if (sampleA.y > sampleB.y)
	{
		region.upper.start =
			interpolateEdgeProfile(
				profileA,
				startT);

		region.upper.end =
			interpolateEdgeProfile(
				profileA,
				endT);

		region.lower.start =
			interpolateEdgeProfile(
				profileB,
				startT);

		region.lower.end =
			interpolateEdgeProfile(
				profileB,
				endT);

		region.ownerRelativeX =
			cellA.relativeX;

		region.ownerRelativeZ =
			cellA.relativeZ;

		region.ownerSide =
			sideA;
	}
	else
	{
		region.upper.start =
			interpolateEdgeProfile(
				profileB,
				startT);

		region.upper.end =
			interpolateEdgeProfile(
				profileB,
				endT);

		region.lower.start =
			interpolateEdgeProfile(
				profileA,
				startT);

		region.lower.end =
			interpolateEdgeProfile(
				profileA,
				endT);

		region.ownerRelativeX =
			cellB.relativeX;

		region.ownerRelativeZ =
			cellB.relativeZ;

		region.ownerSide =
			sideB;
	}

	return region;
}

static XZColumnarEdgeProfile getPlanarCellEdge(
	const XZColumnarPlanarCell& cell,
	XZColumnarSide side)
{
	switch (side)
	{
	case XZColumnarSide::NegativeX:
	{
		return {
			cell.p00,
			cell.p01
		};
	}

	case XZColumnarSide::PositiveX:
	{
		return {
			cell.p10,
			cell.p11
		};
	}

	case XZColumnarSide::NegativeZ:
	{
		return {
			cell.p00,
			cell.p10
		};
	}

	case XZColumnarSide::PositiveZ:
	{
		return {
			cell.p01,
			cell.p11
		};
	}

	default:
	{
		assert(false);
		return {};
	}
	}
}

/***********************************************************
* XZ Columnar Side Region Interface
************************************************************/

glm::vec3 getXZColumnarSideNormal(
	XZColumnarSide side)
{
	switch (side)
	{
	case XZColumnarSide::NegativeX:
		return glm::vec3(-1.0f, 0.0f, 0.0f);

	case XZColumnarSide::PositiveX:
		return glm::vec3(1.0f, 0.0f, 0.0f);

	case XZColumnarSide::NegativeZ:
		return glm::vec3(0.0f, 0.0f, -1.0f);

	case XZColumnarSide::PositiveZ:
		return glm::vec3(0.0f, 0.0f, 1.0f);

	default:
		assert(false);
		return glm::vec3(0.0f, 1.0f, 0.0f);
	}
}

uint32_t buildXZColumnarSharedEdgeSideRegions(
	XZColumnarSideRegion regions[
		MAX_XZ_COLUMNAR_SHARED_EDGE_REGIONS],
		const XZColumnarPlanarCell& cellA,
		XZColumnarSide sideA,
		const XZColumnarPlanarCell& cellB,
		XZColumnarSide sideB)
{
	assert(
		areOpposingColumnarSides(
			sideA,
			sideB));
	
	const XZColumnarEdgeProfile profileA =
		getPlanarCellEdge(
			cellA,
			sideA);

	const XZColumnarEdgeProfile profileB =
		getPlanarCellEdge(
			cellB,
			sideB);

	const float startDifference =
		profileA.start.y -
		profileB.start.y;

	const float endDifference =
		profileA.end.y -
		profileB.end.y;

	const bool startEqual =
		std::abs(startDifference) <=
		XZ_COLUMNAR_SIDE_EPSILON;

	const bool endEqual =
		std::abs(endDifference) <=
		XZ_COLUMNAR_SIDE_EPSILON;

	if (startEqual &&
		endEqual)
	{
		return 0;
	}

	const bool crosses =
		!startEqual &&
		!endEqual &&
		((startDifference < 0.0f) !=
			(endDifference < 0.0f));

	if (!crosses)
	{
		regions[0] =
			buildSideRegion(
				cellA,
				profileA,
				sideA,
				cellB,
				profileB,
				sideB,
				0.0f,
				1.0f);

		return 1;
	}

	const float crossingT =
		startDifference /
		(startDifference - endDifference);

	assert(crossingT > 0.0f);
	assert(crossingT < 1.0f);

	regions[0] =
		buildSideRegion(
			cellA,
			profileA,
			sideA,
			cellB,
			profileB,
			sideB,
			0.0f,
			crossingT);

	regions[1] =
		buildSideRegion(
			cellA,
			profileA,
			sideA,
			cellB,
			profileB,
			sideB,
			crossingT,
			1.0f);

	return 2;
}

XZColumnarClipPolygon getXZColumnarSideRegionPolygon(
	const XZColumnarSideRegion& region)
{
	XZColumnarClipPolygon polygon = {};

	switch (region.ownerSide)
	{
	case XZColumnarSide::NegativeX:
	case XZColumnarSide::PositiveZ:
	{
		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.start });

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.start});

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.end});

		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.end});
	} break;

	case XZColumnarSide::PositiveX:
	case XZColumnarSide::NegativeZ:
	{
		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.start});

		appendXZColumnarClipVertex(
			polygon,
			{ region.upper.end});

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.end});

		appendXZColumnarClipVertex(
			polygon,
			{ region.lower.start});
	} break;

	default:
	{
		assert(false);
		return {};
	} break;
	}

	removeClosingDuplicateXZColumnarClipVertex(
		polygon);

	if (polygon.vertexCount < 3)
	{
		return {};
	}

	glm::vec3 polygonNormal = {};

	if (!calculateXZColumnarClipPolygonNormal(
		polygonNormal,
		polygon))
	{
		return {};
	}

	const glm::vec3 expectedNormal =
		getXZColumnarSideNormal(
			region.ownerSide);

	assert(
		glm::dot(
			polygonNormal,
			expectedNormal) > 0.0f);

	return polygon;
}
