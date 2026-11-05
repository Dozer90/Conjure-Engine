# AI Game Engine

We're building a production-grade, simple and straight-forward 2D + 3D Game Engine for Windows, macOS and Linux (Ubuntu 24+). This is professional-grade software that needs to be stable, fully-tested, and real-world deployable. No hacks, no shortcuts - completely solid. Use a similar setup to the [Shine Engine](A:\Users\jarod\Documents\Git Repos\ShineGameEngine) with CMake and backend abstraction API that can be deployed to any platform/device.

## Dev Workflow
- Ask questions only when absolutely necessary - work autonomously
- IMRORTANT! DO NOT look outside of working directory or use other code on my computer as reference
- Start by asking questions to determine engine name. Suggest a few
- Testing is of the utmost importance - use lots of unit tests (any framework, simple is good), run automated testing
- Setup a scene used for testing, which tests every single feature in the engine - all components, and entire scripting API
- Do things properly, this is production-grade and not a hack project
- Create AGENTS.md and relevant skills to support development, and contain concrete development guidelines
- Code style attached
- Create git repository, commit and push to [GitHub repo](https://github.com/Dozer90/GameEngine_Claude-Assisted.git). IMPORTANT: do a code review before committing, and make sure all changes comply with code style, production-grade quality standards, and have been properly tested/have unit tests that have passed where necessary

## Tech stack
- C++ and CMake for core engine
- Module-based extensions (think 'Unreal Plugin') support for engine expansion and user-generated content
- [GLFW](https://github.com/glfw/glfw) and [nvrhi](https://github.com/NVIDIA-RTX/NVRHI)
- [glm](https://github.com/g-truc/glm) for math
- [miniaudio](https://github.com/mackron/miniaudio) for audio
- C# for scripting, though this can be up for discussion

## Basic Architecture
- Internal system should use DESIGN-ORIENTED DESIGN philosophies over OBJECT-ORIENTED PROGRAMMING
  - Test requirement: should be able to spawn 1000's of dynamic instances with logic and update calls having minimal-to-no impact on CPU/GPU or frame loss
- Public API should abstract DOD to OOP for general users while providing access to true DOD-based methods where the user requires that control (eg: an RTS with hundreds of units, the user will want that control as opposed to those making Tetris)
- Static library for core engine, executable for editor and runtime (depending on chosen design)
- Need to have editor to build games - this can be embedded into runtime executable (and stripped from distribution builds), or can be a standalone executable
- Simple ECS perhaps, to author scene with entities and components
- Ability to "export" game - executable that runs game without editing ability that we can distribute
- Editor needs to be fully controllable by AI agents - I should be able to ask you to build me a game like PacMan, and you should have all the tools available to do so without my intervention

## 3D Renderer
- Import gltf meshes with materials and textures
- Editor has gizmo to position them in the world
- PBR material workflow
- IBL with HDRIs from https://polyhaven.com/hdris
- Soft and hard shadow maps
- Good SSAO
- HDR pipeline with tonemapping

## 3D Physics
- Use simple 3D physics library of your choosing
- Controllable via scripting, authored via components in editor

## Behavior
- Scripting with C# (or other chosen language)
- Control entities/components and run in update loop, and entity destruction/creation
- Spawn new entities/prefabs