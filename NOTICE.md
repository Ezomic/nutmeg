# Notice

Nutmeg is a football game built on its own C++20 engine.

## Licences in this repository

| Area | Licence | SPDX identifier |
| --- | --- | --- |
| `core/`, `platform/` and the tests that cover them | Apache License 2.0 | `Apache-2.0` |
| Everything else (game code, build files, scripts, docs) | GNU GPL v3 or later | `GPL-3.0-or-later` |
| Game data and art | CC0 or CC BY-SA, see `docs/content-licensing.md` | `CC0-1.0`, `CC-BY-SA-4.0` |

Every source file carries an `SPDX-License-Identifier` header. A file without one is covered by the repository default, GPL-3.0-or-later. The full texts are in `LICENSE` (GPL-3.0), `LICENSES/Apache-2.0.txt`, and `core/LICENSE` and `platform/LICENSE`.

Apache-2.0 code may be combined into a GPL-3.0 work, so the combined game is distributed under GPL-3.0-or-later. The Apache-2.0 files stay under Apache-2.0 and can be reused on their own.

## Provenance: Ichor

The generic core and platform code was copied once, in the initial scaffold, from the sibling Ichor project, which has the same author as Nutmeg. It was renamed into the `nutmeg` namespace and has been maintained independently since. The copied areas are:

- `core/`: binary serialisation, deterministic math, file IO, fixed step loop, hashing, input, logging, reflection, random numbers.
- `platform/`: the platform interface and the null platform.
- `tests/`: the tests for the above.

The author owns the copyright in both projects and has licensed the copied code under Apache-2.0 here. Ichor itself is not a dependency and is not linked or vendored.

## Third party software

See `docs/dependencies.md`.
