# Zero Company Mandalorian Wardrobe

Adds the Mandalorian Man001, Man002, and Cly armour sets already shipped with **Star Wars: Zero Company** to the normal colourable wardrobe for human characters.

![A custom blue-and-silver Mandalorian outfit in the Zero Company customization screen](docs/images/wardrobe-preview.png)

## Features

- Sixteen ordinary colourable wardrobe choices across headwear, tops, armwear, lower body, and boots.
- Man001, Man002, and both Cly helmet variants.
- Authored backpacks remain linked to their armour instead of appearing as broken standalone tiles.
- Man001/Man002 helmets are fitted around the live animated head pivot, eliminating the tested face clipping without hiding the character's face.
- Mandalorian helmets use the game's authored helmet voice effect in conversations.
- Exact human-species gating; the mod does not globally unlock every authored or incompatible outfit.
- The game's original compatibility check still makes the final decision after the narrowly required Mandalorian/Cly tags are supplied.
- No console, keybind, save editor, or extracted game assets.

## Requirements

- Windows Steam release of Star Wars: Zero Company, build `24874058`.
- [UE4SS for Star Wars Zero Company v1.0](https://www.nexusmods.com/starwarszerocompany/mods/9).

Yes, **UE4SS is required**. This is a native UE4SS C++ mod. Install the game-specific compatibility build above before installing the wardrobe. The compatibility package is not bundled here because its author does not permit re-uploading it.

The tested files are pinned to these hashes:

| File | SHA-256 |
|---|---|
| `SWZeroCompany.exe` | `C69131D496756EA421E408261FBA33B60613948E2C480ACAC91CB93632A4B67C` |
| compatibility `UE4SS.dll` | `8CB45C18230547A1EAD97BFEB34A2B5EF710B778890DDFC01877A7E9C61A07F4` |
| wardrobe `main.dll` | `CCF08AB5E82CE02ED5016857AA1B322130B95E292ADF6E1F8149A2C0F5FBAF2A` |

The installer and DLL refuse an unrecognised executable or UE4SS build rather than guessing at native addresses.

## Installation — recommended automatic method

1. Close Zero Company completely.
2. Install [UE4SS for Star Wars Zero Company](https://www.nexusmods.com/starwarszerocompany/mods/9):
   - In Steam, right-click **Star Wars: Zero Company**.
   - Choose **Manage → Browse local files**.
   - Open `SWZeroCompany`, then `Binaries`, then `Win64`.
   - Extract the UE4SS compatibility package into that `Win64` folder. When correctly installed, the folder contains `ue4ss\UE4SS.dll`.
3. Download `ZeroCompanyMandoWardrobe-v0.4.0-build24874058.zip` from this repository's Releases page.
4. Right-click the ZIP, choose **Extract All**, and open the newly extracted folder. Do not run the installer from inside the ZIP preview.
5. Double-click **`Install-Wardrobe.cmd`**. Leave its window open until it says `INSTALL COMPLETE`.

The installer automatically searches the default Steam folder and every library listed in Steam's `libraryfolders.vdf`. It verifies the game, UE4SS and mod DLL before changing anything, creates a recoverable backup, and enables the wardrobe. You normally do not need to type or find the game path yourself.

### PowerShell method

If the `.cmd` launcher is blocked, open PowerShell in the **extracted mod folder containing `Install-Wardrobe.ps1`**:

1. Open that folder in File Explorer.
2. Click the address bar at the top of File Explorer.
3. Type `powershell` and press Enter. A PowerShell window opens already pointed at the correct folder.
4. Paste this command and press Enter:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File ".\Install-Wardrobe.ps1"
   ```

Auto-detection is used when `-GameBin` is omitted. If auto-detection cannot find the game, use Steam's **Manage → Browse local files**, open `SWZeroCompany\Binaries\Win64`, copy the full path from File Explorer's address bar, and run:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File ".\Install-Wardrobe.ps1" -GameBin "D:\SteamLibrary\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64"
   ```

The installer verifies all three hashes, preserves unrelated `mods.txt` entries and comments, creates a recoverable backup, copies only the wardrobe DLL, and enables:

```text
ZeroCompanyMandoWardrobe : 1
```

It does not install UE4SS or read or modify save files.

## Manual installation — exact folders

First locate the game through Steam: right-click the game, choose **Manage → Browse local files**, then open:

```text
SWZeroCompany\Binaries\Win64
```

This `Win64` folder is the game binary directory. On a default Steam installation its complete path is:

```text
C:\Program Files (x86)\Steam\steamapps\common\Star Wars Zero Company\SWZeroCompany\Binaries\Win64
```

In the extracted mod download, open `payload`, then `ue4ss`, then `Mods`. Copy the entire `ZeroCompanyMandoWardrobe` folder into the game's:

```text
...\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\ue4ss\Mods
```

The finished DLL must be at exactly:

```text
...\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\ue4ss\Mods\ZeroCompanyMandoWardrobe\dlls\main.dll
```

There should not be a second nested `ZeroCompanyMandoWardrobe` folder. Next, open this file in Notepad:

```text
...\Star Wars Zero Company\SWZeroCompany\Binaries\Win64\ue4ss\Mods\mods.txt
```

Add or update this line:

```text
ZeroCompanyMandoWardrobe : 1
```

Do not replace the entire `mods.txt`, because it may contain other installed mods.

## Using the wardrobe

Load the game normally and open the standard character customization wardrobe. The Mandalorian parts behave like regular wardrobe entries and use the normal per-slot colour controls.

The DLL logs either a `READY` line or a fail-closed `REFUSED` reason to `ue4ss\UE4SS.log` during startup.

If support asks for a runtime report, close the game, open PowerShell in the extracted mod folder using the File Explorer address-bar method above, and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".\Collect-WardrobeRuntime.ps1"
```

The report auto-detects the game and saves a copy of only this mod's log lines under `%LOCALAPPDATA%\ZeroCompanyMandoWardrobe\Diagnostics`.

## Disable or uninstall

Before disabling or removing the mod, equip a normal stock outfit and save. Loading a save while it still references mod-enabled Mandalorian parts without the mod present has not been fully tested.

Disable while retaining the DLL:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Set-WardrobeEnabled.ps1" -State Disabled
```

The safe uninstall command disables the entry and keeps a recoverable copy:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Uninstall-Wardrobe.ps1"
```

After equipping and saving a stock outfit, remove the installed folder into the backup location with:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Uninstall-Wardrobe.ps1" -RemoveFiles -StockOutfitConfirmed
```

## Compatibility and limitations

- Version 0.4.0 supports only Steam build `24874058`. A game update will probably require a newly verified release.
- Man001/Man002 helmet fitting is runtime and animation-aware. A face may be visible very briefly while a newly created preview helmet settles and is fitted.
- Helmet fitting was runtime-tested on masculine Hawks in the wardrobe, hub movement, and conversations. Every human face/body combination has not been visually checked.
- The catalogue logic is gated to the game's exact Human species tag, but every possible human character/body combination has not been visually checked.
- No achievement-disabling code or game-side mod gate was found. Achievements are expected to remain available, but a naturally earned achievement with this exact runtime setup has not yet been observed.

## Source and safety boundary

The complete mod source is in [`src/dllmain.cpp`](src/dllmain.cpp) and [`src/helmet_fit.cpp`](src/helmet_fit.cpp), with reproducible build instructions in [`docs/BUILDING.md`](docs/BUILDING.md).

The mod hooks four exact game functions and validates the executable PE identity plus every native function prologue it relies on before enabling itself. It modifies wardrobe catalogue results only for nineteen explicitly named shipped definitions: sixteen visible parts and three hidden backpack dependencies. Helmet fitting is restricted to the exact Man001/Man002 helmet meshes, keeps authored transforms for restoration, and does not hide or mutate face components. It does not patch the game executable on disk or include extracted game assets.

## Licence and disclaimer

The original source and installer scripts are released under the [MIT License](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md) for the libraries used by the release DLL.

This is an unofficial fan-made mod. It is not affiliated with or endorsed by Bit Reactor, Electronic Arts, Lucasfilm, Disney, Nexus Mods, or the UE4SS project. Star Wars and related marks and assets belong to their respective owners.
