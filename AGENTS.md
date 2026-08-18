# AGENTS.md

This file provides guidance to AI coding agents working in this repository. **Full project guidance lives in `CLAUDE.md` — read it first; this file only captures the essentials.**

## Project: Incogine

A 2D/3D-capable C++ game engine (and reference game) by **leafstudiosDot**, licensed under **MPL-2.0** (Mozilla Public License 2.0).

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

## Quick build (full details in CLAUDE.md)

- Windows: `mkdir build && cd build && cmake .. && cmake --build .`
- macOS/Linux: `mkdir build && cd build && cmake .. && make`
- Engine version lives in `src/core/engine/version.h`, mirrored to `src/project.xml` (`<incogine_version>`).
- Project identity (name, window title, bundle ID, copyright) is regex-extracted from `src/project.xml` by `CMakeLists.txt`.
- Generated files: font/audio C arrays (from `ttfparse*.py`/`audioparse.py`, only with `ICG_EMBED_ASSETS=ON`) and `_svgdata.h` (from `svgparse.py`) all land in the build dir — gitignored, don't edit by hand.