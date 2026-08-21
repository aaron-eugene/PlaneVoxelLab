# Plane Voxel Lab Code Conventions

This document records coding conventions and design rules for the Plane Voxel Lab
codebase. These conventions are intended to keep ownership, lifetime, naming, and
module boundaries clear as the project grows.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Note to ChatGPT

Your role is to advise and support the development of this project, but not to agree by 
default.

If I suggest an approach that seems flawed, risky, unclear, or likely to create problems 
later, say so directly and explain why. If there is a cleaner or more maintainable solution, 
present it.

Disagreement is welcome when it improves the design, clarifies trade-offs, or helps me 
learn. If we disagree, we can discuss the reasoning and decide deliberately.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Current Build Target

- Visual Studio
- Windows x64
- OpenGL 4.5 Core
- GLFW
- GLAD
- GLM
- Dear ImGui

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Style and Formatting

How the code looks.

### Unicode

For now, do not include Unicode characters in project files.

### Naming 

Use names that describe ownership, behavior, and intent.

Names should reflect what a function or type actually does. Prefer distinctions such as
`create`, `build`, `initialize`, `append`, `emit`, `clip`, `sample`, `classify`, `update`,
and `render` when those distinctions are meaningful.

#### Function Naming Semantics

Use `create`, `initialize`, and `make` according to the lifetime and ownership behavior 
of the operation.

##### `create`

Use `create` when establishing an explicitly managed resource or resource-owning object.

Creation typically writes into a destination passed by reference. When replacing a live 
destination would violate its lifetime contract, the destination must be empty before 
creation.

Creation may report runtime failure when the underlying operation has a meaningful 
failure path.

Examples:

```cpp
bool createGpuMesh(GpuMesh& mesh, ...);
bool createTexture2D(Texture2D& texture, ...);
```

##### `initialize`

Use `initialize` when resetting and populating ordinary state.

Initialization may safely replace previous state when normal reset or RAII semantics 
are sufficient. It does not imply ownership of an explicitly managed external resource.

Examples:

```cpp
void initializeChunk(Chunk& chunk, const ChunkCoord& coord);
bool initializeLabWorld(LabWorld& world);
```

##### `make`

Use `make` when constructing and returning a value, adapter, view, or other lightweight 
object that does not require a matching explicit destruction operation.

Examples:

```cpp
DensityField makeSphereDensityField(
	const SphereDensityField& sphere);

DensityField makeHeightmapDensityField(
	const HeightmapDensityField& heightmap);
```

### Capitalization

Type definitions use PascalCase.

Functions, type instances, and variables use camelCase.

No Hungarian notation. 

### Bracing

Prefer Allman-style bracing and indentation.

### Comment Policy

Comments should describe purpose, behavior, ownership, lifetime, or non-obvious design
decisions. 

They should generally be brief and live above the code that they comment.

Avoid comments that merely repeat the code.

Prefer comments that explain why a boundary, order, or contract exists.

### File Headers

Used for both headers and implementation files. 

```cpp

///////////////////////////////////////////////////////////////////////////////
// renderer/shader.h
// =================
//
// Brief module description.
//
///////////////////////////////////////////////////////////////////////////////

```

### Comment Banners

Banners are used for organization of files, functions, types, and any large blocks of code.
Three types may be used. Prefer use of main and tertiary banners when possible. 

#### Main Banners

```cpp

/***********************************************************
* Main Banner
************************************************************/

```

Used in headers for:
- delineating major module types
- grouping functionality like module interfaces

Used in implementation files for:
- grouping helper types or functions
- delineating individual function implementations

#### Secondary Banners

```cpp

//===========================================================
// Secondary Banner
//===========================================================

```

Generally not needed in header files.

Used in implementation files for:
- breaking apart large functions into major sections

#### Tertiary Banners

```cpp

//--------------------------------------------------
// Tertiary Banner
//--------------------------------------------------

```

Used in headers for:
- breaking apart large types into logical sections

Used in implementation files for:
- breaking major sections of a function into smaller, logical sections

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## General Design Philosophy

How the code is shaped.

### Data-Oriented Design

Plane Voxel Lab uses a data-oriented C++ style rather than an object-oriented design.

The preferred structure is:
- plain structs for data
- free functions for behavior
- explicit ownership
- clear module boundaries
- minimal hidden state

### Type Policy

Prefer plain structs with public data.

Structs that own explicitly managed resources should be paired with explicit create/destroy
functions appropriate to their lifetime. Plain state structs, including structs that contain
ordinary RAII containers such as `std::vector`, may use initialize/shutdown functions when
resetting the struct is sufficient to manage its lifetime.

Derived or cached data should be named so that its purpose is clear and should live with
the system that is responsible for maintaining it.

### Function Policy

Prefer free functions that operate on explicit data passed through parameters.

Functions should make ownership and mutation clear through their signatures.

Prefer:

```cpp

bool createGpuMesh(GpuMesh& mesh, const RenderVertex* vertices, uint32_t vertexCount);
void destroyGpuMesh(GpuMesh& mesh);
void renderMesh(const GpuMesh& mesh, const ShaderProgram& shader);

```

Avoid functions that depend on hidden global state unless that state is intentionally
file-local and part of a narrow implementation detail.

### Pointer and Reference Policy

Use references for required inputs and outputs.

Use pointers when null is a meaningful value or when pointer semantics are otherwise
appropriate, such as borrowed array data.

A pointer parameter must have a clear contract:
- nullable, if null is allowed
- required, if null is not allowed and should be asserted against

Raw pointers do not imply ownership unless explicitly documented.

Owning resources should be represented by structs with explicit lifetime functions.

### Global State Policy

Avoid project-wide global state.

File-local static state may be used for constants or truly private implementation details.

Long-lived runtime state should live in explicit state structs.

Prefer passing state explicitly between systems and modules.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Contracts, Errors, and Lifetime

What functions and resources promise and how failure is handled.

### Function Contracts

Functions should have clear contracts.

When a function requires valid input, it should enforce that contract with assertions.

Invalid input caused by incorrect internal usage is a programmer error, not a recoverable
runtime failure.

### Assertion vs Runtime Failure

Use `assert` for programmer errors and broken internal contracts.

Use runtime checks for recoverable failure paths or normal runtime failures.

A broken ownership contract is not the same kind of failure as an external library or
resource failing to initialize. Broken contracts should stop execution during development
so the calling code can be fixed.

### Recoverability Policy

A function should report failure when the caller has a meaningful decision to make.

Recoverable failure does not mean the current caller must recover immediately. During
development, a higher-level caller may still choose to terminate after receiving a failure.

Returning failure from a lower-level function can remain useful because a higher-level system
may later choose to:

- retry
- use a fallback resource
- skip a non-critical feature
- report the error through debug UI or logging
- shut down cleanly

Use assertions for failures that indicate:

- broken contracts
- invalid internal usage
- impossible or invalid system state

Use `bool` return values for failures that the caller may reasonably want to make a decision
about.

Examples of assertion failures:

- passing a null pointer where a valid pointer is required
- passing a non-empty resource to a creation function
- passing zero vertices to mesh creation
- calling a rendering function before renderer initialization
- receiving an invalid OpenGL handle where that indicates broken renderer setup

Examples of recoverable or reportable failures:

- GLFW fails to initialize
- a window cannot be created
- GLAD fails to load OpenGL functions
- shader compilation fails
- shader linking fails
- an image asset cannot be loaded
- a mesh or texture resource cannot be created and the caller may choose a fallback

During early development, assertions may temporarily stand in for recovery paths that do not
exist yet. This is a development-stage choice, not a permanent error-handling strategy.

As fallback resources, logging, debug UI, asset validation, and renderer error systems are
added, some assertions may be converted into recoverable failure paths.

### Error Handling Policy

Use `bool` return values for initialization and creation functions when the caller has a
meaningful decision to make if the function fails.

Use `void` when an operation has no meaningful reportable failure path. Do not return a `bool`
only because an underlying standard-library operation could theoretically fail in a way the
function does not observe.

For this lab, allocation failure from standard containers is not handled explicitly. Standard
containers are used as a convenience, and the project does not add exception-based recovery
or custom allocation handling solely to detect out-of-memory conditions.

On failure, creation functions should clean up any partial resources they created and leave
the destination resource empty.

Do not add failure checks only because a function can theoretically fail. Use assertions for
failures that indicate invalid internal state, invalid API usage, or broken system setup.

### Resource Lifetime Policy

Creation is strict. Destruction is tolerant. Replacement is explicit. Failed creation leaves 
the destination resource empty.

#### Creation Functions

Creation functions for resource-owning structs require an empty destination and assert that
contract.

This is a programmer contract and should be enforced with assertions.

The create function may write into the resource during creation. If creation fails, it cleans
up only the resources created during that attempt and leaves the destination empty.

On success, the destination is left valid.

Creation functions should not silently overwrite live resources. Replacing a live resource
requires explicit destruction first.

Initialization functions for plain state structs may reset the destination to a known default
state. Ordinary RAII-owned members such as `std::vector` do not by themselves require an
empty-destination contract or explicit destruction before reinitialization.

#### Destroy Functions

Destroy and shutdown functions should be safe to call on empty resources.

They should release owned resources and reset the struct afterward.

Destruction should not require the resource to be valid or populated.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Source and Module Boundaries

How files and modules expose, hide, and depend on functionality.

### Module Definition

A module is a cohesive subsystem of the project, usually represented by a source directory.
A module may contain multiple header/implementation pairs when its responsibility requires
several related types or operations.

Individual files should have focused responsibilities, but module ownership and dependency
boundaries are defined at the subsystem level rather than treating every header/implementation
pair as a separate module.

For example, everything under `chunk/` belongs to the chunk module, while individual files
within that directory may separate chunk storage, sampling behavior, or other chunk-specific
responsibilities.

`module_contracts.md` documents these module-level boundaries rather than maintaining a
contract entry for every source file.

### Header Policy

Headers define module contracts.

A header should contain only the types, constants, and function declarations that other
files or modules are allowed to use.

Implementation details should stay in `.cpp` files.

A public type should not expose another module's type unless that dependency is intentional
and part of the public contract.

### Include Policy

Headers should include only what they need to define their public types and function
declarations.

Implementation files should include their matching header first, followed by required
dependencies in this order:

- other project module headers, alphabetically
- third-party library headers, grouped sensibly
- C and C++ standard library headers, alphabetically

Implementation files should include what they use directly rather than relying on
transitive inclusion from their matching header.

Prefer forward declarations when they reduce unnecessary dependencies without obscuring
the public contract.

Do not forward declare external library types unless the type is intentionally opaque at the
module boundary.

Avoid unnecessary includes in headers. Header dependencies should be deliberate because
they become dependencies for every file that includes that header.

Do not use `using namespace` in headers.

### File-Local Visibility Policy

File-local helper functions should use `static`.

File-local helper types may be placed in an anonymous namespace when they should not be
visible outside the implementation file.

Anonymous namespaces should not be placed in headers.

### Module Dependency Policy

Modules should depend only on concepts appropriate to their layer and responsibility.

Higher-level orchestration modules may depend on lower-level shared systems.

Lower-level shared modules should not depend on higher-level orchestration modules or on
specific experiments.

Experiments may depend on shared project systems, but shared systems should not depend on
experiment-specific types.

Avoid circular dependencies between modules.

If a public type begins depending on a higher-level module only to access one small shared
concept, reconsider whether that concept belongs in a lower-level shared module.

### Module Ownership

Each module should clearly define:

- what it owns
- what it creates and destroys
- what it borrows or references
- what concepts belong to it
- what concepts explicitly do not belong to it

See module_contracts.md for module-specific boundaries.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -



