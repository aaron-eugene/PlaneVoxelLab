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
- The shared density surface value and inside/outside classification convention

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

## Lab Module

### Purpose

The Lab module is the top-level terrain experiment workbench.

It owns and coordinates the shared LabWorld, reference surface, debug
visualization state, shared render settings and terrain-render resources, and
the compile-time selected active experiment.

The Lab module is responsible for orchestrating initialization, shutdown,
updates, rendering, debug UI, and rebuilds that affect multiple subsystems.

### Owns

* `Lab`
* Top-level lifetime and orchestration of lab subsystems
* `LabWorld` lifetime
* `SurfaceRef` lifetime
* `ChunkWireframes` lifetime
* `ActiveExperiment` lifetime
* The current `StandardRenderSettings`
* The `TerrainRenderResources` instance used by terrain experiments
* Visibility state for major lab components
* Shared Lab debug UI
* Density-field selection at the workbench level
* Coordination of rebuilds after shared world data changes
* The compile-time bridge between the Lab and the selected active experiment

### Does Not Own

* Definitions or implementations of density fields
* Chunk storage behavior or coordinate conventions
* Surface-map generation behavior
* Reference-surface extraction internals
* Renderer implementation or renderer-owned shader programs
* Terrain-render resource types or atlas vocabulary
* Experiment-specific algorithms or geometry
* Shared debug-resource implementations such as chunk-wireframe mesh creation

### Dependency Boundary

The Lab module may depend on:

* LabWorld
* Surface Reference
* Lab Debug
* Renderer-facing render settings and interfaces
* Terrain Render
* Input
* the active-experiment bridge

`active_experiment.*` is the intentional compile-time boundary through which the
Lab depends on the selected concrete experiment.

Other Lab code should not directly depend on experiment-specific types or
implementation details.

Lower-level shared modules must not depend on the Lab module.

### Lifetime

`Lab` aggregates several subsystems that own explicitly managed resources.

Initialization is intended for a fresh or previously shut-down `Lab`. It resets
ordinary aggregate state before initializing owned subsystems in dependency
order.

If initialization fails, already initialized subsystems are shut down and the
Lab is returned to an empty state.

Shutdown releases owned subsystem resources in reverse dependency order and
resets the Lab.

Runtime rebuild operations may propagate detectable failures upward. The Lab is
not required to recover from unrecoverable runtime resource failures; those
failures may be reported and propagated to the application level for clean
termination.

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

The Renderer module provides the lab's low-level OpenGL rendering layer.

It owns the renderer's shared shader programs and defines the GPU-resource,
vertex-format, texture, shading-setting, and drawing interfaces used by
higher-level systems.

The module may also provide narrowly scoped CPU-side loading utilities used
directly to create renderer resources.

### Owns

- `Renderer`
- Renderer-owned shader programs and cached uniform locations
- `GpuMesh`
- GPU vertex-array, vertex-buffer, and index-buffer resources
- `Texture2D`
- OpenGL texture creation and destruction
- `ShaderProgram`
- Shader-source loading, shader compilation, and program linking
- Renderer-supported CPU vertex formats
- CPU-side RGBA image loading used by renderer texture creation
- Standard render settings and renderer shading modes
- OpenGL primitive and vertex-layout translation
- Renderer frame setup
- Colored-mesh rendering
- Standard textured/shaded mesh rendering

### Does Not Own

- Source terrain or surface geometry
- Chunk, LabWorld, or density-field state
- Surface-reference state
- Active experiment state
- Camera state
- Terrain tile or atlas semantics
- Higher-level render-resource orchestration
- Experiment-specific visualization policy
- Debug visualization ownership outside renderer resources

### Dependency Boundary

The Renderer module may depend on:

- OpenGL
- GLM
- image-decoding support used by renderer image loading
- standard-library facilities used for resource loading and data handling

It must not depend on:

- chunk storage
- spatial/world systems
- density fields
- surface systems
- LabWorld
- active experiments
- terrain-render semantics
- higher-level lab orchestration

Higher-level systems may create renderer resources and request drawing through
the Renderer API, but they should not depend on renderer-owned shader
implementation details.

Higher-level visualization policy should select among renderer capabilities
rather than introducing experiment-specific behavior into the renderer.

### Lifetime

Renderer-owned and GPU-backed resource types use explicit creation and
destruction.

Creation requires an empty destination.

Successful creation leaves a complete valid resource. Detectable runtime
creation failure returns `false` and leaves the destination empty.

Destruction is tolerant of empty or partially created resources and resets the
destination to its empty state.

CPU-side `ImageData` owns its decoded pixel allocation until explicitly
destroyed.

`Renderer` initialization creates its required shared shader resources. On
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

The Surface Map module builds and stores derived lookup data identifying where
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

The Surface Map module may depend on:

- spatial/grid definitions
- shared voxel geometry/topology
- chunk density-sample storage and access
- the shared density inside/outside classification defined by Fields

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
- the shared density inside/outside classification defined by Fields

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

## Terrain Render Module

### Purpose

The Terrain Render module defines shared terrain-rendering vocabulary and owns
the resource types and helpers used to render terrain with the shared tile
atlas.

It provides the terrain tile definitions, atlas layout and UV mapping, and the
shared GPU texture resource used by terrain experiments.

### Owns

* `TerrainTile`
* Terrain tile atlas dimensions and layout
* Terrain tile atlas UV mapping
* `TerrainRenderResources`
* Loading and validating the terrain tile atlas image
* Creation and destruction of the terrain tile atlas GPU texture
* The asset path and texture-sampling configuration used for the shared terrain
  atlas

### Does Not Own

* Lab state or orchestration
* Active experiment state
* Terrain geometry or mesh generation
* Renderer or shader lifetime
* Chunk or LabWorld data
* Density fields
* Surface maps or reference surfaces
* Rules that determine which terrain tile an experiment assigns to a surface

### Dependency Boundary

The Terrain Render module may depend on:

* renderer image-loading and texture-resource interfaces
* GLM types required for UV calculations
* standard-library facilities

It must not depend on:

* `Lab`
* `LabWorld`
* chunks or density fields
* surface systems
* specific terrain experiments
* higher-level lab orchestration

Terrain experiments may depend on the Terrain Render module for shared terrain
tile vocabulary, atlas mapping, and rendering resources.

### Lifetime

`TerrainRenderResources` owns explicitly managed GPU texture resources.

Creation requires an empty destination.

Successful creation leaves a complete valid resource set. Detectable runtime
creation failure returns `false` and leaves the resource set empty.

Destruction is tolerant of empty state and releases all owned GPU resources
before resetting the destination.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## XZ Columnar Experiment Module

### Purpose

The XZ Columnar Experiment module implements an experimental terrain-surface
representation that approximates a heightmap with voxel-sized X/Z planar cells.

Each planar cell is constructed from the tangent plane at the center of a
bicubic Hermite height patch. The resulting top surfaces and exposed side
regions are clipped and partitioned by chunk and voxel Y boundaries so that
emitted geometry has explicit voxel ownership.

The experiment also assigns voxel-local terrain tile UVs and maintains the CPU
and GPU mesh state required to render the resulting terrain.

### Owns

- `XZColumnarExperiment`
- XZ columnar build settings and diagnostic statistics
- CPU-side XZ columnar meshes and ownership metadata
- GPU meshes created for the experiment
- Bicubic height-patch sampling used by planar-cell construction
- Planar-cell construction and one-cell X/Z halo grids
- Tangent-plane surface approximation
- Height-based terrain-tile selection for planar cells
- Temporary clipping polygon types and Y-slab clipping
- Shared-edge comparison and exposed side-region construction
- X/Z planar-cell ownership of exposed side regions
- Y-voxel slicing and ownership of top pieces and side fragments
- Voxel-local terrain tile UV generation
- Experiment-specific debug UI and visualization settings

### Does Not Own

- `Lab` or Lab orchestration
- `LabWorld`
- Heightmap density-field data
- `SurfaceMap`
- Shared spatial coordinate conventions
- Shared voxel topology
- Terrain tile definitions, atlas layout, or atlas texture resources
- Renderer implementation or renderer-owned resources
- Shared shader programs or render settings
- Reference-surface generation

The experiment borrows source world, heightmap, surface-map, renderer, and
terrain-render data while building or rendering.

### Dependency Boundary

The XZ Columnar Experiment may depend on:

- Fields for heightmap sampling
- Spatial for voxel/chunk dimensions and coordinates
- Surface Map for identifying relevant chunk columns
- Renderer CPU vertex formats and GPU mesh interfaces
- Terrain Render for terrain tile vocabulary, atlas UV mapping, and shared
  terrain-render resources
- GLM and standard-library facilities

It must not be depended on by lower-level shared modules.

The Lab interacts with the experiment through the compile-time
`active_experiment` bridge rather than directly depending on XZ-columnar
implementation details.

The experiment should not introduce XZ-columnar-specific concepts into shared
world, surface, renderer, or terrain-render modules unless a later experiment
demonstrates that the concept is genuinely reusable.

### Geometry and Ownership

A planar-cell grid represents the X/Z cells owned by one chunk column together
with a one-cell X/Z halo used for neighboring-edge comparisons.

Each planar cell stores a tangent-plane approximation derived from a bicubic
height patch sampled at the cell center.

Top polygons are clipped first to the owning chunk's Y slab and then to
individual voxel Y slabs. Each emitted top piece records its owning
`VoxelCoord`.

Neighboring planar-cell edge profiles are compared to discover exposed side
regions. Each region is assigned to the cell whose surface lies above the
other within that region. Side regions are subsequently clipped by chunk and
voxel Y slabs, and each emitted side fragment records its owning voxel and
outward side.

### Lifetime

`XZColumnarExperiment` owns its generated CPU meshes and corresponding
explicitly managed GPU meshes.

Initialization requires an empty experiment and performs an initial rebuild.

Rebuilding replaces the existing generated mesh state. CPU mesh construction
does not report recoverable failure. Detectable GPU-resource creation failure
returns `false` and leaves the experiment without partially committed render
meshes.

Shutdown destroys all owned GPU resources, clears generated CPU and diagnostic
state, and resets the experiment.

The planar-cell grid and clipping polygons are temporary CPU-side construction
data and own no external resources.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
