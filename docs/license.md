---
title: License
description: The MPL-2.0 licensing model and project direction decisions for Incogine.
sidebar_position: 16
tags: [license, mpl-2.0, legal]
---

# License

Incogine is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**. The official text is in [`LICENSE`](../LICENSE). The old custom "Incogine License" and `COMMERCIAL_LICENSE.md` were removed.

## What MPL-2.0 means here

- **Games built with Incogine may be closed source.** Game/project code (e.g. `src/project/`, `src/scenes/`) is a "Larger Work" under MPL — it can stay proprietary. Only the engine itself (Covered Software) carries obligations.
- **The engine core can never be closed.** MPL file-level copyleft: anyone who modifies engine files and distributes them must publish those modifications under MPL. This is the maintainer's core concern — nobody can take the engine proprietary.
- **Attribution.** All files must retain `leafstudiosDot`/`Incogine`. A "Powered by Incogine" startup splash is desired but is **not** license-mandated.
- **Community over revenue.** No commercial license fees, no royalties, no EULA. Donations are optional.

## Project direction decisions

Recorded with the maintainer on 2026-08-16:

- **Community over revenue.** No commercial fees or royalties; donations are optional (a GitHub Sponsors link can be added to the README once set up).
- **Games may be closed source** (see above).
- **The engine core can never be closed** (see above).
- **Attribution** must be retained everywhere.
- **Trademark plan (not yet registered):** once the business is registered, file an "Incogine" trademark and publish a short brand policy page (name/logo usage) separate from the license. Until then, no trademark enforcement.
- **Rejected alternatives — do not revisit without a new discussion with the maintainer:**
  - MIT/Apache (a modified core could be closed)
  - GPL/AGPL (would force games open)
  - Custom source-available EULA with contribution-back-by-email clauses (unenforceable, kills community, rejected by corporate legal)
  - Upfront commercial fees (the $100/mo model was removed as "greed")

## See also

- [Contributing](./contributing.md) — the contributor license agreement and attribution rules.