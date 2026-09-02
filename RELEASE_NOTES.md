# Zero Company Mandalorian Wardrobe v0.4.2

Wardrobe and helmet-fit update for the Windows Steam build `24874058` of **Star Wars: Zero Company**.

## What changed

- Slightly enlarges only the Man001 helmet horizontally to remove the remaining cheek intersection. Man002 keeps its previously tested fit.
- Keeps the render-palette implementation that follows movement, missions, and cutscenes without moving helmet components or changing attachments.
- Retains all sixteen colourable Man001, Man002, and Cly wardrobe choices, authored backpack dependencies, and helmet voice effects.
- Strengthens catalogue injection and live-object validation during scene changes and shutdown.
- Replaces the old script-based package with a standard direct-install mod ZIP.

The underlying render-palette fit completed extended testing covering Den movement, dialogues, cutscenes, missions, and a full campaign. The v0.4.2 change is deliberately limited to a one-percent horizontal adjustment for Man001.

## Install or update

Install [UE4SS for Star Wars Zero Company v1.0](https://www.nexusmods.com/starwarszerocompany/mods/9) first. UE4SS is required and is not bundled with this release.

Download `ZeroCompanyMandoWardrobe-v0.4.2-build24874058.zip`, open it, and drag its `ue4ss` folder into the game's `SWZeroCompany\Binaries\Win64` folder. Allow Windows to merge folders and replace the older mod DLL when updating. No PowerShell or command prompt is required.

## Known limitations

- Only Steam build `24874058` is supported by this release.
- A newly created helmet may briefly appear at its authored size before the bounded mesh registry identifies it.
- Every possible human character and body combination has not been visually tested.
- Close the game before installing, updating, disabling, or removing the native DLL; do not hot-reload it in a running UE4SS session.

The mod contains no extracted game assets. Source code is available in the repository under the MIT License.
