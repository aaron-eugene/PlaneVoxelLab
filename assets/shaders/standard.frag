///////////////////////////////////////////////////////////////////////////////
// standard.frag
// =============
//
// Fragment shader for shared standard mesh shading.
//
///////////////////////////////////////////////////////////////////////////////

#version 450 core

in vec3 worldNormal;
in vec3 vertexColor;
in vec2 tileUv;

uniform int uShadingMode;

uniform vec3 uLightDirection;
uniform float uAmbientStrength;
uniform float uDiffuseStrength;

uniform sampler2D uTexture;

out vec4 outColor;

/***********************************************************
* Standard Shading Modes
************************************************************/

const int SHADING_MODE_TEXTURED_UNLIT = 0;
const int SHADING_MODE_TEXTURED_LIT = 1;
const int SHADING_MODE_VERTEX_COLOR = 2;
const int SHADING_MODE_NORMAL_VISUALIZATION = 3;

void main()
{
	vec3 normal =
		normalize(worldNormal);

	if (uShadingMode ==
		SHADING_MODE_NORMAL_VISUALIZATION)
	{
		vec3 normalColor =
			normal * 0.5 +
			vec3(0.5);

		outColor =
			vec4(
				normalColor,
				1.0);

		return;
	}

	if (uShadingMode ==
		SHADING_MODE_VERTEX_COLOR)
	{
		outColor =
			vec4(
				vertexColor,
				1.0);

		return;
	}

	vec3 textureColor =
		texture(
			uTexture,
			tileUv).rgb;

	vec3 baseColor =
		textureColor *
		vertexColor;

	if (uShadingMode ==
		SHADING_MODE_TEXTURED_UNLIT)
	{
		outColor =
			vec4(
				baseColor,
				1.0);

		return;
	}

	if (uShadingMode ==
		SHADING_MODE_TEXTURED_LIT)
	{
		// uLightDirection is the direction in which the light rays travel.
		vec3 surfaceToLightDirection =
			normalize(-uLightDirection);

		float diffuseFactor =
			max(
				dot(
					normal,
					surfaceToLightDirection),
				0.0);

		float lightStrength =
			clamp(
				uAmbientStrength +
				uDiffuseStrength *
					diffuseFactor,
				0.0,
				1.0);

		outColor =
			vec4(
				baseColor *
					lightStrength,
				1.0);

		return;
	}

	// Fall back to unlit textured color if an invalid mode reaches the shader.
	outColor =
		vec4(
			baseColor,
			1.0);
}