# renderer-wrapper

Rendering workspace of a 2D **metroidvania game engine** (sibling workspaces: ECS, engine, UI).

## Purpose

Provide a **backend-agnostic rendering interface** so the backend can be swapped without touching
engine or UI code.

- **bgfx** — the primary/target backend.
- **SDL3 CPU rendering** — the interim backend the UI team uses (drag & drop, entity creation)
  until the bgfx path is usable; then we switch behind the same interface.

## Non-negotiable constraints

### CLion is the IDE — the play button must just work
The project must build and run from CLion's green play button, with **no manual pre-step**
(no hand-run `shaderc`, no copying assets, no `cd` to a specific directory).
Anything the build needs belongs in `CMakeLists.txt`.
Note: CLion runs the binary from the **build directory** (`cmake-build-debug`) by default —
never rely on the source dir being the working directory.

### Claude never writes to git
Git is the human's alone. Claude never runs `git commit`, `git push`, `git merge`, `git rebase`,
`git reset --hard`, or anything else that changes history, the index, or a remote — not even
when asked to "save" or "finish" work, and not on any branch. Claude edits files in the working
tree and stops there; the human reviews and commits.
Read-only git (`status`, `diff`, `log`, `show`) is fine, as is `git mv` for a rename that is
part of the change being made.

`.claude/` is local tooling configuration, not project code: it is gitignored and stays that way.

### The human drives CLion's UI
Claude does **not**:
- create C++ files or classes (CLion's *New > C++ Class* does it, with the project's file header template),
- build or compile the project (the human clicks build/run and pastes the output).

Claude **does**: tell the human exactly which files/classes to create (name + directory),
then write their contents, and edit `CMakeLists.txt` and shaders.

## Code standards

- C++20, `-Wall -Wextra -Wpedantic` (MSVC `/W4`) must stay clean.
- **Modularity first**: small classes with one clear purpose, communicating through well-defined
  interfaces. If a file grows large, it is doing too much — split it.
- No backend type (`bgfx::`, `SDL_Renderer*`) leaks through a public interface header.
- RAII for every GPU/native resource; no leaking handles, no manual cleanup left to the caller.
- Keep the existing file-header comment banner on new files (CLion template).

### Comments
- Comment only what the code cannot say: a non-obvious constraint, a trap, or *why* this way.
- Never restate what the line already says, and never narrate obvious code.
- **One line per comment.** If it needs a paragraph, the code needs a better name or a split.

## Layout

**One unit, one directory** — anything with a `.cpp` lives in its own folder named after it,
holding its `.hpp` and `.cpp`. Header-only units (interfaces, POD types) sit at the group root.
Related units are grouped under a parent folder.

```
main.cpp                            entry point: SDL3 window + loop
Renderer/                           group: everything rendering
  RendererTypes.hpp                 header-only -> group root
  IRenderer.hpp                     header-only interface -> group root
  SdlRenderer/SdlRenderer.{hpp,cpp} has a .cpp -> own directory
  BgfxRenderer/, BgfxProgram/       bgfx backend (default; --backend=sdl for SDL)
  QuadBuilder/, CameraMatrix/       backend-free, in renderer_core, unit-tested
  ...
Tools/                              group: backend-free helpers (e.g. ImageLoader: PNG -> RGBA8)
Playground/                         group: playground-only code (e.g. Player, ModeToggle, Showcase)
Tests/                              GoogleTest files, flat: <Unit>Tests.cpp (no header, so no own folder)
assets/                             data only (images); copied next to the binary at build time
shaders/                            .sc sources (varying.def.sc, vs_sprite.sc, fs_sprite.sc)
```

## Workflow

Follow the superpowers skills: brainstorm before building, plan before coding, verify before
claiming anything works (paste-back of the actual CLion build/run output counts as evidence).
Look up bgfx/SDL3 APIs via context7 rather than from memory.
