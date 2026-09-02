# Zero Company Mandalorian Wardrobe

Adds the shipped Man001, Man002, and Cly Mandalorian armour parts to the wardrobe as normal, colourable customization choices for human characters.

## Features

- Sixteen exact armour parts across headwear, tops, armwear, lower body, and boots.
- Authored backpack dependencies remain attached to their matching torso instead of appearing as separate tiles.
- Mandalorian helmets use the game's helmet voice effect.
- Man001 and Man002 helmets receive a render-only fit around the animated head pivot. The mod does not move scene components or hide faces.
- Global equipment validation, saves, classes, missions, and unrelated wardrobe content are left unchanged.

## Requirements

- Windows Steam build `24874058` of **Star Wars: Zero Company**.
- [UE4SS for Star Wars Zero Company v1.0](https://www.nexusmods.com/starwarszerocompany/mods/9), installed first.

UE4SS is required and is not included in this download.

## Install or update

1. Close the game.
2. In Steam, right-click **Star Wars: Zero Company**, choose **Manage**, then **Browse local files**.
3. In the window that opens, enter `SWZeroCompany`, then `Binaries`, then `Win64`.
4. Download `ZeroCompanyMandoWardrobe-v0.4.2-build24874058.zip` from the [Releases page](https://github.com/Sternab/ZeroCompanyMandoWardrobe/releases).
5. Open the ZIP and drag its `ue4ss` folder into the game's `Win64` folder.
6. If Windows asks, choose to merge the folders and replace the existing mod file.
7. Launch the game normally.

No PowerShell, command prompt, or `mods.txt` editing is required. The included `enabled.txt` marker activates the mod.

After installation these files should exist:

```text
SWZeroCompany\Binaries\Win64\ue4ss\Mods\ZeroCompanyMandoWardrobe\enabled.txt
SWZeroCompany\Binaries\Win64\ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll
```

Updating from an older release uses the same steps. Copy the new `ue4ss` folder over the old one and allow the DLL to be replaced.

## Remove

1. Equip stock wardrobe parts before removing the mod.
2. Close the game.
3. Delete `SWZeroCompany\Binaries\Win64\ue4ss\Mods\ZeroCompanyMandoWardrobe`.

If a much older scripted installer added `ZeroCompanyMandoWardrobe : 1` to `ue4ss\Mods\mods.txt`, that leftover line may also be removed or changed to `0`.

## Troubleshooting

- Confirm UE4SS itself is installed in the same `Win64` folder.
- Confirm the two installed paths shown above exist and are not nested inside an extra ZIP-named folder.
- This release supports only Steam build `24874058`; it deliberately refuses an unverified executable build.
- Install, update, or remove the native DLL only while the game is closed. Do not hot-reload it.
- For support, attach `SWZeroCompany\Binaries\Win64\ue4ss\UE4SS.log` after reproducing the problem.

## Known limitations

- A newly created helmet can briefly appear at its authored size before the bounded render registry identifies it.
- Every possible human character and body combination has not been visually tested.
- A game update may require a newly verified release.

## Source and safety

The mod contains no extracted game assets. It appends only an exact whitelist of shipped customization definitions, delegates compatibility to the game's original checks, and applies helmet fitting only to the exact Man001 and Man002 mesh objects. Source is available under the MIT License.
