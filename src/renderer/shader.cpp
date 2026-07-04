///////////////////////////////////////////////////////////////////////////////
// renderer/shader.cpp
// ===================
//
// Implements shader source loading, OpenGL shader compilation, and shader
// program linking helpers.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/shader.h"

#include <glad/glad.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>

/***********************************************************
* File-Local Helpers
************************************************************/

static bool loadTextFile(std::string& text, const char* filePath)
{
	assert(text.empty());
	assert(filePath != nullptr);

	std::ifstream file(filePath);
	if (!file.is_open())
	{
		std::printf("Failed to open text file: %s\n", filePath);
		return false;
	}

	std::stringstream buffer = {};
	buffer << file.rdbuf();

	text = buffer.str();
	return true;
}

static uint32_t compileShader(uint32_t shaderType, const char* shaderSource)
{
	assert(shaderSource != nullptr);

	uint32_t shader = glCreateShader(shaderType);

	// Invalid handles here indicate broken renderer setup or invalid OpenGL state.
	assert(shader != 0);

	glShaderSource(shader, 1, &shaderSource, nullptr);
	glCompileShader(shader);

	int compileSucceeded = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSucceeded);

	if (!compileSucceeded)
	{
		char infoLog[2048] = {};
		glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);

		std::printf("Failed to compile shader:\n%s\n", infoLog);

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static void printProgramLinkError(uint32_t shaderProgram)
{
	assert(shaderProgram != 0);

	char infoLog[2048] = {};
	glGetProgramInfoLog(shaderProgram, sizeof(infoLog), nullptr, infoLog);

	std::printf("Failed to link shader program:\n%s\n", infoLog);
}

/***********************************************************
* Shader Source Interface
************************************************************/

bool loadShaderSource(
	ShaderSource& shaderSource,
	const char* vertexShaderPath,
	const char* fragmentShaderPath)
{
	assert(shaderSource.vertexSource.empty());
	assert(shaderSource.fragmentSource.empty());

	assert(vertexShaderPath != nullptr);
	assert(fragmentShaderPath != nullptr);

	if (!loadTextFile(shaderSource.vertexSource, vertexShaderPath))
	{
		destroyShaderSource(shaderSource);
		return false;
	}

	if (!loadTextFile(shaderSource.fragmentSource, fragmentShaderPath))
	{
		destroyShaderSource(shaderSource);
		return false;
	}

	return true;
}

void destroyShaderSource(ShaderSource& shaderSource)
{
	shaderSource = {};
}

/***********************************************************
* Shader Lifecycle
************************************************************/

bool createShaderProgram(
	ShaderProgram& shaderProgram,
	const ShaderSource& shaderSource)
{
	assert(shaderProgram.handle == 0);
	assert(!shaderSource.vertexSource.empty());
	assert(!shaderSource.fragmentSource.empty());

	const bool shaderCreated = createShaderProgram(
		shaderProgram,
		shaderSource.vertexSource.c_str(),
		shaderSource.fragmentSource.c_str());

	return shaderCreated;
}

bool createShaderProgram(
	ShaderProgram& shaderProgram,
	const char* vertexShaderSource,
	const char* fragmentShaderSource)
{
	assert(shaderProgram.handle == 0);
	assert(vertexShaderSource != nullptr);
	assert(fragmentShaderSource != nullptr);

	uint32_t vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
	if (vertexShader == 0)
	{
		return false;
	}

	uint32_t fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
	if (fragmentShader == 0)
	{
		glDeleteShader(vertexShader);
		return false;
	}

	uint32_t program = glCreateProgram();

	// Invalid handles here indicate broken renderer setup or invalid OpenGL state.
	assert(program != 0);

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	int linkSucceeded = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linkSucceeded);

	if (!linkSucceeded)
	{
		printProgramLinkError(program);

		glDeleteProgram(program);
		return false;
	}

	shaderProgram.handle = program;
	return true;
}

void destroyShaderProgram(ShaderProgram& shaderProgram)
{
	if (shaderProgram.handle != 0)
	{
		glDeleteProgram(shaderProgram.handle);
	}

	shaderProgram = {};
}

/***********************************************************
* Shader Access Interface
************************************************************/

int getShaderUniformLocation(
	const ShaderProgram& shaderProgram,
	const char* uniformName)
{
	assert(shaderProgram.handle != 0);
	assert(uniformName != nullptr);

	const int location =
		glGetUniformLocation(shaderProgram.handle, uniformName);

	if (location < 0)
	{
		std::printf(
			"Failed to find shader uniform: %s\n",
			uniformName);
	}

	return location;
}