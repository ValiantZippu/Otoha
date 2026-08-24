# Otoha License

Otoha is free software: you can redistribute it and/or modify it under the
terms of the **GNU Affero General Public License v3.0** (AGPL-3.0-or-later),
as published by the Free Software Foundation.

SPDX-License-Identifier: AGPL-3.0-or-later

Canonical license text: https://www.gnu.org/licenses/agpl-3.0.txt

## Why this license

Otoha builds on JUCE, which is dual-licensed (AGPLv3 / commercial). The
AGPLv3 option keeps Otoha fully open-source at zero licensing cost while
remaining compliant with JUCE's terms for our distribution model. See
docs/licensing.md for the full dependency audit.

## Before tagging a public release (maintainer checklist)

- [ ] Vendor the complete, verbatim AGPLv3 text into this file (or a plain
      `LICENSE` file) from https://www.gnu.org/licenses/agpl-3.0.txt —
      the full text must ship with source distributions.
- [ ] Re-confirm no dependency was added that conflicts with AGPLv3.
- [ ] Keep JUCE usage under the AGPLv3 option consistent with how the app is
      distributed (source availability obligations).

## Third-party components

See `THIRD-PARTY-NOTICES` / docs/licensing.md. FFmpeg is *not* distributed
with Otoha; users install their own build under its own license.
