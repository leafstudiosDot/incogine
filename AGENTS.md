# AGENTS.md

This file provides guidance to AI coding agents working in this repository. **Full project guidance lives in `CLAUDE.md` — read it first; this file only captures the essentials.**

## Project: Incogine

A 2D/3D-capable C++ game engine (and reference game) by **leafstudiosDot**, licensed under **MPL-2.0** (Mozilla Public License 2.0).

**Two builds in one repo:** `Incogine` is the **engine core** build; `Puroko` is a **game-project** build — a reference example of any project someone can make on Incogine (like your own game's project layer). Keep this distinction in mind when documenting or changing either.

## Documentation policy

**`docs/` must stay in sync with the code.** Any change that touches something not already documented in `docs/` (new features, new files, changed behavior, new build steps) must also update `docs/` — add or revise the relevant Docusaurus-compatible markdown page(s). Never leave undocumented changes behind.

## Licensing (see CLAUDE.md "Licensing & Project Direction" for the full record)

- **License: MPL-2.0.** Official text in `LICENSE`. The old custom "Incogine License" and `COMMERCIAL_LICENSE.md` were removed.
- **Community over revenue** — no commercial fees, no royalties, no EULA. Donations are optional.
- **Games built on Incogine may be closed source** (game code is a "Larger Work" under MPL).
- **The engine core can never be closed** — MPL file-level copyleft: modified engine files must be published under MPL. This is the maintainer's core concern.
- **Attribution:** retain `leafstudiosDot`/`Incogine` in all files (see `CONTRIBUTING.md`). A "Powered by Incogine" startup splash is desired but NOT license-mandated.
- **Trademark "Incogine" is not yet registered** — plan: register business, then file trademark, then publish a brand policy page.
- **Rejected alternatives (do not revisit without a new discussion):** MIT/Apache, GPL/AGPL, custom source-available EULA with contribution-back clauses, upfront commercial fees.
- **Keep `CLAUDE.md` updated:** whenever licensing is discussed, record the decisions in the "Licensing & Project Direction" section.

## Git policy

**Never run git commands that write to the repository or alter history** — no commits, pushes, amends, rebases, resets, branch operations, stashes, cleans, or checkouts that modify files. Read-only commands (`git status`, `git diff`, `git log`) are fine. All repository changes stay in the working tree until the maintainer commits them.

## Submodules / third-party deps (`reqs/`, `emsdk`)

- **Read-only:** never create, edit, or delete files inside submodule checkouts listed in `.gitmodules` (`emsdk/`, `reqs/SDL3_*_source/`, any future `reqs/*` entry) or inside gitignored prebuilt drops in `reqs/` (`SDL3`, `SDL3_ttf`, `SDL3_image`, `SDL3_mixer`, etc.). Read them for reference; build against them via `CMakeLists.txt`; fix integration issues in our own code/CMake, not by patching upstream.
- **Allowed: bump to newer upstream release tags.** `git fetch --tags` / `git submodule update --remote --checkout <path>` (or `git checkout <release-tag>` inside the submodule) to test the latest stable `release-*` tag matching the tracked branch in `.gitmodules` (e.g. SDL `release-3.4.x`). Verify with a build. Do not retarget URLs/branches in `.gitmodules`, and do not commit or push — leave the moved gitlink in the working tree for the maintainer to review and commit.
- After re-extracting Windows prebuilts, recreate the `reqs/SDL3_mixer/include/SDL3_mixer/SDL_mixer.h` shim if the package still uses the legacy header layout (see CLAUDE.md build notes).

## Quick build (full details in CLAUDE.md)

- Windows: `mkdir build && cd build && cmake .. && cmake --build .`
- macOS/Linux: `mkdir build && cd build && cmake .. && make`
- Engine version lives in `src/core/engine/version.h`, mirrored to `src/project.xml` (`<incogine_version>`).
- Project identity (name, window title, bundle ID, copyright) is regex-extracted from `src/project.xml` by `CMakeLists.txt`.
- Generated files: font/audio C arrays (from `ttfparse*.py`/`audioparse.py`, only with `ICG_EMBED_ASSETS=ON`) and `_svgdata.h` (from `svgparse.py`) all land in the build dir — gitignored, don't edit by hand.