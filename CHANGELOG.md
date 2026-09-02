# Changelog

## 0.4.2 — 2026-09-02

- Increases only the Man001 helmet's horizontal render-palette fit from `1.06` to `1.07`, removing the remaining minor cheek intersection without changing its height or attachment pivot.
- Keeps Man002 at its previously tested `1.06` horizontal fit.
- Moves wardrobe injection to the final customization-definition catalogue seam and continues to append only the exact sixteen whitelisted parts after the game's own compatibility checks.
- Adds constant-time live-object validation and guarded native registry calls to make late scene transitions and teardown safer.
- Replaces the public script-based package with a direct `Win64` drop-in ZIP and an `enabled.txt` activation marker. No PowerShell or command scripts are required.

## 0.4.1 — 2026-08-29

- Replaces the v0.4.0 scene-component helmet fit, which could leave a helmet behind during mission or cutscene animation, with a render-palette-only fit.
- Fits only the exact Man001/Man002 helmet meshes after the game builds their current and previous skinning palettes, keeping animation and motion-vector frames coherent.
- Removes all helmet scene-transform writes; attachments, component poses, face visibility, and saves remain untouched.
- Runtime-validated through Den movement, conversations, cutscenes, and a complete mission with 405,706 current and 815 previous palette applications and zero ABI refusals.
- Updates the support collector to diagnose the render-palette implementation and reject legacy transform markers.

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
