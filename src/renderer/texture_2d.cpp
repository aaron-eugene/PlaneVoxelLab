///////////////////////////////////////////////////////////////////////////////
// renderer/texture_2d.cpp
// =======================
//
// Implements generic OpenGL 2D texture creation and destruction.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/texture_2d.h"

#include <glad/glad.h>

#include <cassert>

/***********************************************************
* Texture 2D Helpers
************************************************************/

static GLint getOpenGlTextureFilter(
	TextureFilter filter)
{
	switch (filter)
	{
	case TextureFilter::Nearest:
	{
		return GL_NEAREST;
	}

	case TextureFilter::Linear:
	{
		return GL_LINEAR;
	}
	}

	assert(false);
	return GL_NEAREST;
}

static GLint getOpenGlTextureWrap(
	TextureWrap wrap)
{
	switch (wrap)
	{
	case TextureWrap::Repeat:
	{
		return GL_REPEAT;
	}

	case TextureWrap::ClampToEdge:
	{
		return GL_CLAMP_TO_EDGE;
	}
	}

	assert(false);
	return GL_CLAMP_TO_EDGE;
}

/***********************************************************
* Texture 2D Interface
************************************************************/

bool createTexture2D(
	Texture2D& texture,
	const Texture2DCreateInfo& createInfo)
{
	assert(texture.handle == 0);
	assert(texture.width == 0);
	assert(texture.height == 0);

	assert(createInfo.width > 0);
	assert(createInfo.height > 0);
	assert(createInfo.rgbaPixels != nullptr);

	GLuint textureHandle = 0;

	glCreateTextures(
		GL_TEXTURE_2D,
		1,
		&textureHandle);

	if (textureHandle == 0)
	{
		return false;
	}

	glTextureParameteri(
		textureHandle,
		GL_TEXTURE_MIN_FILTER,
		getOpenGlTextureFilter(createInfo.minFilter));

	glTextureParameteri(
		textureHandle,
		GL_TEXTURE_MAG_FILTER,
		getOpenGlTextureFilter(createInfo.magFilter));

	glTextureParameteri(
		textureHandle,
		GL_TEXTURE_WRAP_S,
		getOpenGlTextureWrap(createInfo.wrapS));

	glTextureParameteri(
		textureHandle,
		GL_TEXTURE_WRAP_T,
		getOpenGlTextureWrap(createInfo.wrapT));

	glTextureStorage2D(
		textureHandle,
		1,
		GL_RGBA8,
		static_cast<GLsizei>(createInfo.width),
		static_cast<GLsizei>(createInfo.height));

	glTextureSubImage2D(
		textureHandle,
		0,
		0,
		0,
		static_cast<GLsizei>(createInfo.width),
		static_cast<GLsizei>(createInfo.height),
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		createInfo.rgbaPixels);

	texture.handle = textureHandle;
	texture.width = createInfo.width;
	texture.height = createInfo.height;

	return true;
}

void destroyTexture2D(
	Texture2D& texture)
{
	if (texture.handle != 0)
	{
		const GLuint textureHandle = texture.handle;

		glDeleteTextures(
			1,
			&textureHandle);
	}

	texture = {};
}
