///////////////////////////////////////////////////////////////////////////////
// renderer/image_data.h
// =====================
//
// Defines owned CPU-side image data loaded from an image file.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Image Data Types
************************************************************/

struct ImageData
{
	uint8_t* rgbaPixels = nullptr;

	uint32_t width = 0;
	uint32_t height = 0;
};

/***********************************************************
* Image Data Interface
************************************************************/

bool loadImageData(
	ImageData& image,
	const char* path);

void destroyImageData(
	ImageData& image);
