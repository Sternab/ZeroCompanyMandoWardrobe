# Changelog

## 0.4.0 — 2026-08-29

- Fits the Man001/Man002 helmets horizontally around their live animated head-bone pivot, removing the tested face clipping during wardrobe previews, movement, and conversations.
- Isolates transient preview-component failures so a component still being constructed cannot undo verified live helmet fits.
- Adds the game's authored helmet voice effect to the four Mandalorian/Cly helmet definitions when the original solver has no preset.
- Removes the experimental face-visibility workaround; faces are no longer hidden or mutated by the mod.
- Adds Steam-library auto-detection to every package helper that accepts a game path.
- Adds a double-click installer and explicit PowerShell, Steam browser, and manual folder instructions.

## 0.3.0 — 2026-08-28

- Adds sixteen shipped Man001, Man002, and Cly armour parts as ordinary colourable wardrobe choices for human characters.
- Keeps the three associated backpack definitions as hidden dependencies rather than separate wardrobe tiles.
- Uses an exact human-species gate and passes adapted tags back through the game's original compatibility check.
- Leaves global equipment validation and invalid-part cleanup unchanged.
- Adds fail-closed executable identity and hook-byte validation for Steam build 24874058.
- Includes reversible PowerShell installation, enable/disable, backup restore, and uninstall helpers.
