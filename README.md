# Quick Loot (OBRE)

## Install

Download the archive for your version of the game:

- `OblivionQuickLoot-<version>-Steam.zip`
- `OblivionQuickLoot-<version>-GamePass.zip`

### With a mod manager

- **Vortex:** install the archive as usual. It is recognized as a root mod and deployed to the game's
  `Binaries` folder.
- **Mod Organizer 2 (Steam version):** requires the Root Builder plugin. Install the archive as usual; MO2 moves it into the
  mod's `Root` folder. Use Root Builder's copy or link mode, because a DLL loaded at game start can't be
  provided through MO2's virtual file system. MO2's Oblivion Remastered support only covers the Steam
  version; on Game Pass, use Vortex or install manually.

### Manually

Extract the archive into the game's install folder (the folder that contains `OblivionRemastered`), or copy
its files into the game's `Binaries` folder yourself:

**Steam:**
`…\steamapps\common\Oblivion Remastered\OblivionRemastered\Binaries\Win64\`

**Game Pass / Xbox app:**
`C:\XboxGames\The Elder Scrolls IV- Oblivion Remastered\Content\OblivionRemastered\Binaries\WinGDK\`

The mod's files are `dsound.dll`, `OblivionQuickLoot.dll` and `OblivionQuickLoot.ini`, plus its documentation
(`OblivionQuickLoot_*.md/.txt`). Then launch the game normally.

## Uninstall

Remove it in your mod manager, or delete `dsound.dll` and the `OblivionQuickLoot*` files from that folder.

## Compatibility

- Compatible with UE4SS or any other mod loader.
- Conflicts with any other mod that also installs a `dsound.dll` in the same folder.

## Credits

- **MinHook** by Tsuda Kageyu, for function hooking (BSD 2-Clause, see `THIRD_PARTY_NOTICES.txt`, packaged as `OblivionQuickLoot_THIRD_PARTY_NOTICES.txt`).
