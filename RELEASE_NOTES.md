# Zero Company Mandalorian Wardrobe v0.4.1

Helmet-fit hotfix for the Windows Steam build `24874058` of **Star Wars: Zero Company**.

## What changed

- Replaces the v0.4.0 scene-component fit that could leave a helmet behind during mission and cutscene animations.
- Fits the exact Man001/Man002 helmet meshes in their final current and previous render palettes around the animated head pivot.
- Never moves helmet components, changes attachments, or hides character faces.
- Retains all sixteen colourable Man001, Man002, and Cly wardrobe choices, authored backpack dependencies, and helmet voice effects.
- Updates the packaged runtime collector for the new implementation.

The new fit completed a test covering Den movement, dialogues, cutscenes, and a mission with 405,706 current-palette and 815 previous-palette applications, zero ABI refusals, no legacy component-transform events, and no face-visibility mutation.

## Requirement

Install [UE4SS for Star Wars Zero Company v1.0](https://www.nexusmods.com/starwarszerocompany/mods/9) first. UE4SS is required and is not bundled with this release.

Download `ZeroCompanyMandoWardrobe-v0.4.1-build24874058.zip`, extract it, and double-click `Install-Wardrobe.cmd`. The installer auto-detects Steam libraries and verifies the supported game, UE4SS, and mod builds before changing anything.

## Known limitations

- Only Steam build `24874058` is supported by this release.
- A newly created helmet may briefly appear at its authored size before the bounded mesh registry identifies it.
- Every possible human character and body combination has not been visually tested.
- Close the game before installing, updating, disabling, or removing the native DLL; do not hot-reload it in a running UE4SS session.

The mod contains no extracted game assets. Source code is available in the repository under the MIT License.
