# Runes Code Conventions

This document records coding conventions and design rules for the Runes codebase. These 
conventions are intended to keep ownership, lifetime, and module boundaries clear as the 
project grows.

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

For now, we are not including unicode characters in any files. 

### Naming 

Use names that describe ownership and intent.

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
// render/shader.h
// ===============
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
- breaking apart very large functions into major sections

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

Runes uses a data-oriented C++ style. No OOP. 

The preferred structure is:
- plain structs for data
- free functions for behavior
- explicit ownership
- clear module boundaries
- minimal hidden state

### Type Policy

Prefer plain structs with public data.

Resource-owning structs should be paired with explicit create/destroy functions.

### Function Policy

Prefer free functions that operate on explicit data passed through parameters.

Functions should make ownership and mutation clear through their signatures.

Prefer:

```cpp

bool createGpuMesh(GpuMesh& mesh, const RenderVertex* vertices, uint32_t vertexCount);
void destroyGpuMesh(GpuMesh& mesh);
void renderMesh(const GpuMesh& mesh, const ShaderProgram& shader);

```

Avoid functions that depend on hidden global state unless the state is explicitly file-local
and temporary during bootstrap.

### Pointer and Reference Policy

Use references for required inputs and outputs.

Use pointers when null is a meaningful value or when passing arrays.

A pointer parameter must have a clear contract:
- nullable, if null is allowed
- required, if null is not allowed and should be asserted against

Raw pointers do not imply ownership unless explicitly documented.

Owning resources should be represented by structs with explicit create/destroy functions.

### Global State Policy

Avoid project-wide global state.

File-local static/global state may be used during early bootstrap or for truly private
implementation details, but long-lived system state should eventually live in explicit
state structs.

Prefer passing state explicitly between systems.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Contracts, Errors, and Lifetime

What functions and resources promise and how failure is handled.

### Function Contracts

Functions should have clear contracts.

When a function requires valid input, the function should enforce that contract with assertions.
Invalid input caused by incorrect internal usage is a programmer error, not a recoverable
runtime failure.

### Assertion vs Runtime Failure

Use assert for programmer errors and broken internal contracts.

Use if statements for recoverable failure paths or for normal runtime failures.

A broken ownership contract is not the same kind of failure as a library failing to load. 
Broken contracts should stop execution during development so the calling code can be fixed.

### Recoverability Policy

A function should report failure when the caller has a meaningful decision to make.

Recoverable failure does not mean the current caller must recover immediately. In early
development, the current caller may assert or terminate on failure. Returning failure from a
lower-level function can still be useful because a higher-level system may later choose to
retry, use a fallback resource, skip a non-critical feature, or shut down cleanly.

Use assertions for failures that indicate broken contracts, invalid internal usage, or an
invalid system state.

Use `bool` return values for failures that the caller may reasonably want to make a decision
about.

Examples of assertion failures:
- passing a null pointer where a valid pointer is required
- passing a non-empty resource to a creation function
- passing zero vertices to mesh creation
- calling a rendering function before renderer initialization
- receiving an invalid OpenGL handle in a situation that indicates broken renderer setup

Examples of recoverable or reportable failures:
- GLFW fails to initialize
- a window cannot be created
- GLAD fails to load OpenGL functions
- shader compilation fails
- shader linking fails
- a mesh or texture resource cannot be created and the caller may choose a fallback

During early development, assertions may temporarily stand in for recovery paths that do not
exist yet. This is a development-stage choice, not a permanent error-handling strategy. As
fallback resources, logging, debug UI, asset validation, and renderer error systems are added,
some assertions may be converted into recoverable failure paths.

### Error Handling Policy

Use `bool` return values for initialization and creation functions when the caller has a
meaningful decision to make if the function fails.

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

These is a programmer contract and should be enforced with assertions.

The create function may write into the resource during creation. If creation fails, the create 
function cleans up only the resources it created during that attempt. On failure, the 
destination is left empty. On success, the destination is left valid.

Creation functions should not silently overwrite live resources. Replacing a live resource 
requires explicit destruction first.

Initialization functions for plain state structs may reset the destination to a known default 
state.

#### Destroy Functions

Destroy/shutdown functions should be safe to call on empty resources.

Destroy functions release any owned resources and reset the struct afterward.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

## Source and Module Boundaries

How files and modules expose or hide things.

### Header Policy

Headers define module contracts.

A header should contain only the types and function declarations that other files/modules 
are allowed to use.

Implementation details should stay in `.cpp` files.

### Include Policy

Headers should include only what they need to define their public types and function
declarations.

Implementation files should include their matching header first, followed by required
dependencies, in a specific order:
- other project module headers, alphabetically
- OpenGL-related libraries (GLAD, GLM, GLFW, ImGui)
- C standard library headers, alphabetically

Prefer forward declarations when possible, but do not forward declare external library
types unless the type is intentionally opaque at the module boundary.

Avoid unnecessary includes in headers. Header dependencies should be deliberate because
they become dependencies for every file that includes that header.

Do not use `using namespace` in headers.

Implementation files should include what they need directly, rather than relying on 
what their shared header provides. 

### File-Local Visibility Policy

File-local helper functions should use `static`.

File-local helper types, such as private structs, should be placed in an anonymous namespace.

Anonymous namespaces should not be placed in headers.

### Module Ownership

Each module should clearly define what it owns and what it only references.

Please see module_contracts.md for details on all modules. 

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -



