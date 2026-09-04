///////////////////////////////////////////////////////////////////////////////
// renderer/image_data.cpp
// =======================
//
// Implements CPU-side RGBA image loading and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/image_data.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>
#include <limits>
#include <cstdint>

/***********************************************************
* Image Data Interface
************************************************************/

bool loadImageData(
	ImageData& image,
	const char* path)
{
	assert(image.rgbaPixels == nullptr);
	assert(image.width == 0);
	assert(image.height == 0);

	assert(path != nullptr);
	assert(path[0] != '\0');

	int loadedWidth = 0;
	int loadedHeight = 0;

	stbi_uc* loadedPixels =
		stbi_load(
			path,
			&loadedWidth,
			&loadedHeight,
			nullptr,
			STBI_rgb_alpha);

	if (loadedPixels == nullptr)
	{
		return false;
	}

	if (loadedWidth <= 0 ||
		loadedHeight <= 0 ||
		static_cast<uint64_t>(loadedWidth) >
		std::numeric_limits<uint32_t>::max() ||
		static_cast<uint64_t>(loadedHeight) >
		std::numeric_limits<uint32_t>::max())
	{
		stbi_image_free(loadedPixels);
		return false;
	}

	image.rgbaPixels =
		loadedPixels;

	image.width =
		static_cast<uint32_t>(
			loadedWidth);

	image.height =
		static_cast<uint32_t>(
			loadedHeight);

	return true;
}

void destroyImageData(
	ImageData& image)
{
	if (image.rgbaPixels != nullptr)
	{
		stbi_image_free(
			image.rgbaPixels);
	}

	image = {};
}
