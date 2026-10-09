# Quick Loot (OBRE)

## Install

Copy the three files from the archive into the game's `Binaries` folder:

- `dsound.dll`
- `OblivionQuickLoot.dll`
- `OblivionQuickLoot.ini`

**Steam:**
`…\steamapps\common\Oblivion Remastered\OblivionRemastered\Binaries\Win64\`

**Game Pass / Xbox app:**
`C:\XboxGames\The Elder Scrolls IV- Oblivion Remastered\Content\OblivionRemastered\Binaries\WinGDK\`

The archive has both folder layouts, so you can also extract the matching folder (`Steam` or `GamePass`)
over the game's install folder. Then launch the game normally.

## Uninstall

Delete `dsound.dll` and the `OblivionQuickLoot.*` files from that folder.

## Compatibility

- Compatible with UE4SS or any other mod loader.
- Conflicts with any other mod that also installs a `dsound.dll` in the same folder.

## Credits

- **MinHook** by Tsuda Kageyu, for function hooking (BSD 2-Clause, see `THIRD_PARTY_NOTICES.txt`).
