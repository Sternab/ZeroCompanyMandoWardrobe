# Zero Company Mandalorian Wardrobe v0.4.0

Helmet-fit, voice, and installer update for the Windows Steam build `24874058` of **Star Wars: Zero Company**.

## What it adds

- Sixteen colourable Man001, Man002, and Cly wardrobe choices for human characters.
- Both Cly helmet variants alongside the two Mandalorian sets.
- The authored backpacks as hidden dependencies, so they remain attached to the intended armour instead of appearing as broken wardrobe tiles.
- Animation-aware Man001/Man002 helmet fitting that removes the tested face clipping without hiding the character's face.
- The game's authored helmet voice effect for the four Mandalorian/Cly helmets.

## Requirement

Install [UE4SS for Star Wars Zero Company v1.0](https://www.nexusmods.com/starwarszerocompany/mods/9) first. UE4SS is required and is not bundled with this release.

Download `ZeroCompanyMandoWardrobe-v0.4.0-build24874058.zip`, extract it, and double-click `Install-Wardrobe.cmd`. The installer auto-detects Steam libraries and verifies the supported game, UE4SS, and mod builds before changing anything.

## Known limitations

- A newly created preview helmet may show the unfitted mesh very briefly before the safe delayed fit is applied.
- Only Steam build `24874058` is supported by this release.
- Every possible human character and body combination has not been visually tested.

The mod contains no extracted game assets. Source code is available in the repository under the MIT License.
