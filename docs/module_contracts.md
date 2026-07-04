# Runes Module Contracts

This document records module ownership and dependency boundaries for stable 
project modules.

The document should stay lightweight. Modules can be expanded here as their 
boundaries become clear.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Renderer Module

The renderer module owns renderer-wide drawing behavior and OpenGL render state.

### Owns

- Renderer-wide OpenGL state
- Frame begin/end behavior
- Mesh drawing behavior
- Renderer-owned shader programs or resources, when explicitly stored in `Renderer`

### Does Not Own

- The GLFW window
- ImGui lifetime
- Game simulation state
- World/chunk data
- Debug geometry data
- GPU mesh resources owned by another system

### May Include

- `render/shader.h`
- `render/gpu_mesh.h`
- GLAD/OpenGL headers
- GLM headers

### Must Not Include

- Debug geometry headers
- Game/world headers
- GLFW headers
- ImGui headers

### Notes

The renderer should draw generic render resources. It should not depend on debug 
geometry or game-specific data. Higher-level code is responsible for passing 
renderable resources into the renderer.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Shader Module

The shader module owns OpenGL shader program creation and destruction.

### Owns

- `ShaderProgram`
- Shader compilation helpers
- Shader program linking helpers
- Shader program destruction

### Does Not Own

- Renderer state
- Mesh resources
- Shader source storage
- Window/context creation

### May Include

- GLAD/OpenGL headers
- Standard headers needed for assertions, integer types, and diagnostic output

### Must Not Include

- Renderer headers
- GPU mesh headers
- Debug geometry headers
- Game/world headers
- GLFW headers
- ImGui headers

### Notes

Shader source pointers are borrowed for the duration of creation. A successfully 
created `ShaderProgram` owns the OpenGL program handle and must be destroyed 
with `destroyShaderProgram()`.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## GPU Mesh Module

The GPU mesh module owns OpenGL mesh resource creation and destruction.

### Owns

- `GpuMesh`
- Vertex array object
- Vertex buffer
- Index buffer
- Mesh upload behavior
- Mesh destruction behavior

### Does Not Own

- CPU-side source vertex arrays
- CPU-side source index arrays
- Shader programs
- Renderer-wide state
- Debug geometry data

### May Include

- Shared geometry data types
- GLAD/OpenGL headers
- Standard headers needed for assertions, offsets, and integer types

### Must Not Include

- Renderer headers
- Shader headers
- Debug geometry headers
- Game/world headers
- GLFW headers
- ImGui headers

### Notes

`createGpuMesh()` borrows CPU-side vertex and index data only for the duration 
of the upload. A successfully created `GpuMesh` owns its OpenGL objects and must 
be destroyed with `destroyGpuMesh()`.
