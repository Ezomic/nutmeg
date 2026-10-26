# Dependencies

Every dependency in use or planned, with its licence and whether it can be distributed with a GPL-3.0-or-later game. Licences were recorded from each project's published licence file and have not been legally reviewed. Re-check the licence at the pinned version when a dependency is added.

| Dependency | Status | Licence (SPDX) | GPL-3.0 distribution | Notes |
| --- | --- | --- | --- | --- |
| SDL3 | Planned (platform layer) | `Zlib` | Compatible | Permissive. |
| EnTT | Planned (game ECS) | `MIT` | Compatible | Header only. |
| miniaudio | Planned (audio) | `Unlicense` or `MIT-0` (choose either) | Compatible | Single header, dual licensed. |
| ozz-animation | Planned (animation) | `MIT` | Compatible | |
| RmlUi | Planned (UI) | `MIT` | Compatible | Needs FreeType, see below. |
| FreeType (via RmlUi) | Planned, transitive | FreeType License (`FTL`) or `GPL-2.0-only` | Compatible under the FTL | Choose the FTL. The GPL-2.0-only option is not compatible with GPL-3.0. Flagged: confirm which FreeType build RmlUi pulls in. |
| Catch2 | In use (tests) | `BSL-1.0` | Compatible | Test builds only, never shipped in the game. Fetched by CMake at v3.7.1. |
| nlohmann-json | Planned (data packs) | `MIT` | Compatible | |
| Dear ImGui | Planned (debug tooling) | `MIT` | Compatible | Debug builds. |
| Tracy | Planned (profiling) | `BSD-3-Clause` | Compatible | Profiling builds. |

## Result

No listed dependency is incompatible with GPL-3.0 distribution. One item needs a check: FreeType must be used under the FreeType License rather than its GPL-2.0-only option.

## Internal code

The Apache-2.0 code in `core/` and `platform/` is compatible with GPL-3.0 (one way: Apache-2.0 into GPL-3.0). Do not move GPL code into those directories, since they must stay Apache-2.0 for reuse.

## Rules for new dependencies

- Prefer permissive licences (MIT, BSD, Zlib, BSL-1.0, Apache-2.0).
- Avoid GPL-2.0-only, AGPL, and any licence with a field-of-use restriction or a non-commercial clause.
- Add the row to this table in the same PR that adds the dependency.
