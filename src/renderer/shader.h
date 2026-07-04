///////////////////////////////////////////////////////////////////////////////
// renderer/shader.h
// =================
//
// Declares OpenGL shader program resources and helper functions for loading,
// compiling, and linking shader programs.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>

/***********************************************************
* Shader Source
************************************************************/

struct ShaderSource
{
	std::string vertexSource = {};
	std::string fragmentSource = {};
};

/***********************************************************
* Shader Program
************************************************************/

struct ShaderProgram
{
	uint32_t handle = 0;
};

/***********************************************************
* Shader Source Interface
************************************************************/

bool loadShaderSource(
	ShaderSource& shaderSource,
	const char* vertexShaderPath,
	const char* fragmentShaderPath);

void destroyShaderSource(ShaderSource& shaderSource);

/***********************************************************
* Shader Lifecycle
************************************************************/

bool createShaderProgram(
	ShaderProgram& shaderProgram,
	const ShaderSource& shaderSource);

bool createShaderProgram(
	ShaderProgram& shaderProgram,
	const char* vertexShaderSource,
	const char* fragmentShaderSource);

void destroyShaderProgram(ShaderProgram& shaderProgram);

/***********************************************************
* Shader Access Interface
************************************************************/

int getShaderUniformLocation(
	const ShaderProgram& shaderProgram,
	const char* uniformName);
