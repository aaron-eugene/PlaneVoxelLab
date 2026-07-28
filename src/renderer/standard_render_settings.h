///////////////////////////////////////////////////////////////////////////////
// renderer/standard_render_settings.h
// ==================================
//
// Declares shared settings used by the renderer's standard shading path.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/vec3.hpp>

/***********************************************************
* Standard Shading Modes
************************************************************/

enum class StandardShadingMode
{
	UnlitVertexColor = 0,
	LitVertexColor = 1,
	NormalVisualization = 2,
};

/***********************************************************
* Standard Render Settings
************************************************************/

struct StandardRenderSettings
{
	StandardShadingMode shadingMode =
		StandardShadingMode::UnlitVertexColor;

	// World-space direction in which the directional light rays travel.
	// glm::normalize(glm::vec3(-0.5f, -1.0f, -0.35f))
	glm::vec3 lightDirection =
		glm::vec3(
			-0.431934f,
			-0.863868f,
			-0.302354f);
	
	float ambientStrength = 0.3f;
	float diffuseStrength = 0.7f;
};
