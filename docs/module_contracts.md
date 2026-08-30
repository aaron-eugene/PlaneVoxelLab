# Plane Voxel Lab Module Contracts

This document records module ownership and dependency boundaries for stable 
project modules.

The document should stay lightweight. Modules can be expanded here as their 
boundaries become clear - it is a living document.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Chunk Module

### Purpose
    
The chunk module owns chunk-local density sample storage and the operations that
populate and access that storage.

### Owns

- `Chunk`
- Chunk coordinates stored with each chunk
- Density sample storage for each chunk
- Chunk density-sample access
- Sampling a borrowed `DensityField` into chunk storage

### Does Not Own

- Density-field definitions or generation parameters
- World-level collections of chunks
- Surface extraction results
- Experiment-specific data
- Rendering or GPU resources

### Dependency Boundary

The chunk module may depend on lower-level spatial/grid definitions and the shared
density-field interface.

It must not depend on:

- `LabWorld` state or orchestration
- surface extraction systems
- specific experiments
- rendering systems

`DensityField` is borrowed during sampling. The chunk module does not own it.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Fields Module

### Purpose

The fields module defines the generic scalar density-field interface and the
procedural field generators used to sample terrain in world space.

### Owns

- `DensityField`
- Density-field sampling and validity checks
- `SphereDensityField`
- `HeightmapDensityField`
- Sphere and heightmap field generation behavior
- Continuous height sampling for heightmap fields

### Does Not Own

- World or chunk storage
- Density samples stored in chunks
- Surface extraction results
- Experiment-specific state
- Rendering or GPU resources

### Dependency Boundary

The fields module may depend on lower-level mathematical utilities such as noise
generation.

It must not depend on:

- chunk storage
- `LabWorld` state or orchestration
- surface extraction systems
- specific experiments
- rendering systems

`DensityField` is a borrowed interface. Its `userData` is not owned by the fields
module and must remain valid for the duration of sampling.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Geometry Module

### Purpose

The geometry module defines shared geometric and topological conventions used
to interpret voxel cells.

### Owns

- Canonical voxel corner indexing
- Canonical voxel edge definitions
- Canonical voxel face definitions and winding
- Voxel corner, edge, and face counts
- Voxel-local corner and center metric offsets

### Does Not Own

- World, chunk, voxel, or sample coordinates
- Chunk or density-sample storage
- Surface extraction algorithms or results
- Experiment-specific geometry
- Rendering or GPU resources

### Dependency Boundary

The geometry module may depend on lower-level spatial/grid definitions needed
to interpret voxel dimensions in metric space.

It must not depend on:

- chunk storage
- `LabWorld` state or orchestration
- surface extraction systems
- specific experiments
- rendering systems

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Lab Debug Module

### Purpose

The Lab Debug module provides shared debugging and diagnostic utilities used by
the lab, including chunk-boundary visualization and lightweight code-section
timing helpers.

### Owns

- `ChunkWireframes`
- The reusable GPU line mesh used to render loaded chunk boundaries
- The list of loaded chunk coordinates used by the chunk-wireframe debug view
- Chunk-wireframe initialization, rendering, and shutdown behavior
- Lightweight elapsed-time measurement helpers
- Optional timing output to standard output

### Does Not Own

- LabWorld state
- Chunk storage
- Surface-map data
- Density fields
- Active experiment state
- Renderer or shader lifetime
- General application UI state
- ImGui controls owned by other systems

### Dependency Boundary

The Lab Debug module may depend on:

- chunk data required to initialize debug visualizations
- shared spatial/grid definitions
- renderer mesh/resource interfaces needed for debug drawing
- standard-library timing facilities

It must not depend on:

- surface-reference implementations
- active experiments
- higher-level lab orchestration
- experiment-specific geometry or rendering state

### Lifetime

`ChunkWireframes` owns an explicitly managed GPU mesh.

Initialization requires an empty destination and creates the reusable chunk
wireframe mesh once for the currently loaded chunk set.

Shutdown releases the owned GPU resource and resets the debug state.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Lab World Module

### Purpose

The lab world module owns and maintains the generated world state shared by the
surface reference and active terrain experiments.

It coordinates density-field selection, chunk generation and sampling, and the
surface map derived from the sampled chunks.

### Owns

- `LabWorld`
- Active density-field selection
- Sphere and heightmap generator state used by the lab world
- The borrowed `DensityField` view bound to the active generator state
- The world-level collection of sampled chunks
- The `SurfaceMap` derived from those chunks
- Lab-specific world loading configuration

### Does Not Own

- Generic spatial coordinate types or coordinate-conversion rules
- Generic voxel/chunk grid constants
- Density-field implementations
- Individual chunk sampling behavior
- Surface-reference geometry
- Experiment-specific state
- Rendering or GPU resources
- Lab debug state

### Dependency Boundary

The lab world module may depend on lower-level:

- spatial/grid infrastructure
- chunk storage and sampling
- density fields
- shared surface-map data

It must not depend on:

- specific experiments
- surface-reference implementations
- rendering systems
- higher-level lab orchestration

### Lifetime Notes

`LabWorld::densityField` is a borrowed view whose `userData` points to generator
state stored inside the same `LabWorld`.

Because that internal pointer is not automatically rebound by ordinary struct copying
or moving, an initialized `LabWorld` must remain at a stable address and must not be
copied or moved while its density-field view is bound.

`LabWorld` is initialized and used in place for the lifetime of the lab.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Math Module

### Purpose

The math module provides low-level mathematical helpers and deterministic
procedural-noise functions shared by higher-level systems.

### Owns

- General interpolation and smoothing helpers
- Deterministic 2D value noise
- Fractal value-noise composition
- Internal hashing used by procedural noise

### Does Not Own

- Terrain or density-field configuration
- World, chunk, voxel, or sample coordinates
- Surface extraction
- Experiment-specific behavior
- Rendering or GPU resources

### Dependency Boundary

The math module is a low-level shared module and should depend only on the
standard library or similarly fundamental facilities.

It must not depend on:

- spatial/world infrastructure
- chunk storage
- density fields
- `LabWorld`
- surface systems
- experiments
- rendering systems

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Renderer Module

### Purpose

The Renderer module owns the lab's core OpenGL rendering resources and provides
the mesh-upload and drawing interfaces used by higher-level systems.

It defines the renderer-supported vertex formats, GPU mesh resources, shader
program resources, texture resources, standard render settings, and frame-level
rendering operations.

### Owns

- `Renderer`
- Renderer-owned shader programs and cached uniform locations
- `GpuMesh`
- GPU vertex-array, vertex-buffer, and index-buffer resources
- `ShaderProgram`
- Shader-source loading, shader compilation, and program linking
- Renderer-supported CPU vertex formats
- Texture resources and texture upload helpers
- Standard render settings and renderer shading modes
- OpenGL primitive-type translation
- Renderer frame setup
- Colored-mesh rendering
- Standard textured/shaded mesh rendering

### Does Not Own

- Source terrain or surface geometry
- Chunk, LabWorld, or density-field state
- Surface-reference state
- Active experiment state
- Camera state
- Higher-level render-resource orchestration
- Debug visualization ownership outside renderer resources

### Dependency Boundary

The Renderer module may depend on:

- OpenGL
- GLM
- standard-library facilities used for resource loading and data handling

It must not depend on:

- chunk storage
- spatial/world systems
- density fields
- surface systems
- LabWorld
- active experiments
- higher-level lab orchestration

Higher-level systems may create renderer resources and request drawing through
the Renderer API, but they should not depend on renderer-owned shader
implementation details.

### Lifetime

Renderer-owned and GPU-backed resource types use explicit creation and
destruction.

Creation requires an empty destination.

Successful creation leaves a complete valid resource. Detectable runtime
creation failure returns `false` and leaves the destination empty.

Destruction is tolerant of empty or partially created resources and resets the
destination to its empty state.

`Renderer` initialization creates its required shader resources. On
initialization failure, `Renderer` is left empty. Shutdown releases all
renderer-owned resources and resets the renderer.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Spatial Module

### Purpose

The spatial module defines the shared voxel/chunk grid model, coordinate types,
and canonical conversions between grid-local and world-space representations.

### Owns

- World-unit and voxel-size constants
- Chunk dimensions and sample-grid dimensions
- `ChunkCoord`
- `VoxelCoord`
- `SampleCoord`
- Coordinate validation
- Chunk/world/local position conversions
- Voxel-local position helpers
- Voxel and sample indexing conventions

### Does Not Own

- Chunk storage or density samples
- LabWorld loading limits or world orchestration
- Voxel corner/edge/face topology
- Density fields
- Surface extraction
- Experiment-specific state
- Rendering or GPU resources

### Dependency Boundary

The spatial module is foundational shared infrastructure.

It may depend on fundamental math/vector types required to represent positions.

It must not depend on:

- chunk storage
- density fields
- geometry topology
- `LabWorld`
- surface systems
- experiments
- rendering systems

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Surface Map Module

### Purpose

The surface module builds and stores derived lookup data identifying where
sampled chunk density data contains surface crossings.

It organizes surface-bearing chunks into contiguous X/Z column ranges for use
by surface extraction and terrain systems.

### Owns

- `SurfaceMap`
- `SurfaceChunk`
- `SurfaceChunkColumn`
- `SurfaceVoxel`
- Surface-crossing classification
- Surface-chunk ordering and X/Z column lookup data
- Rebuilding derived surface lookup data from sampled chunks

### Does Not Own

- Source chunk storage or density samples
- Density-field definitions or generator state
- Voxel topology
- LabWorld orchestration
- Extracted surface geometry
- Experiment-specific state
- Rendering or GPU resources

### Dependency Boundary

The surface module may depend on:

- spatial/grid definitions
- shared voxel geometry/topology
- chunk density-sample storage and access

It must not depend on:

- `LabWorld` state or orchestration
- surface-reference implementations
- specific experiments
- rendering systems

Source chunks are borrowed during SurfaceMap rebuilding. The SurfaceMap stores
derived lookup data and does not retain references or pointers to those chunks.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Surface Reference Module

### Purpose

The Surface Reference module builds, owns, and renders the lab's reference
surface from sampled chunk density data.

The current reference surface is generated using marching tetrahedra.

### Owns

- `SurfaceRef`
- `SurfaceRefChunk`
- CPU-side reference surface meshes
- GPU meshes uploaded for reference-surface rendering
- Reference-surface chunk rebuild logic
- Reference-surface rendering
- Marching-tetrahedra reference mesh generation
- Marching-tetrahedra decomposition tables and implementation-specific helpers

### Does Not Own

- Source chunk storage or density samples
- Density-field definitions or generator state
- `SurfaceMap` data
- LabWorld orchestration
- Renderer or shader lifetime
- Experiment-specific terrain state
- Shared spatial or voxel-topology definitions

### Dependency Boundary

The Surface Reference module may depend on:

- chunk density-sample storage and access
- spatial/grid definitions
- shared voxel geometry/topology
- `SurfaceMap` lookup data
- renderer mesh/resource interfaces needed for upload and drawing

It must not depend on:

- `LabWorld` orchestration
- active experiments
- experiment-specific geometry or rendering state
- higher-level lab orchestration

### Lifetime

`SurfaceRef` owns explicitly managed GPU resources through its
`SurfaceRefChunk` entries.

Initialization requires an empty destination.

Rebuilding replaces the existing reference surface. On successful rebuild,
`SurfaceRef` contains the complete replacement surface. On rebuild failure,
`SurfaceRef` is left empty.

Shutdown releases all owned GPU resources and resets the `SurfaceRef`.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

