///////////////////////////////////////////////////////////////////////////////
// renderer/texture_2d.h
// =====================
//
// Defines a generic owned OpenGL 2D texture resource.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Texture 2D Types
************************************************************/

enum class TextureFilter
{
	Nearest = 0,
	Linear,
};

enum class TextureWrap
{
	Repeat = 0,
	ClampToEdge,
};

struct Texture2DCreateInfo
{
	uint32_t width = 0;
	uint32_t height = 0;

	const uint8_t* rgbaPixels = nullptr;

	TextureFilter minFilter = TextureFilter::Nearest;
	TextureFilter magFilter = TextureFilter::Nearest;

	TextureWrap wrapS = TextureWrap::ClampToEdge;
	TextureWrap wrapT = TextureWrap::ClampToEdge;
};

struct Texture2D
{
	uint32_t handle = 0;

	uint32_t width = 0;
	uint32_t height = 0;
};

/***********************************************************
* Texture 2D Interface
************************************************************/

bool createTexture2D(
	Texture2D& texture,
	const Texture2DCreateInfo& createInfo);

void destroyTexture2D(
	Texture2D& texture);
