"""Build the release archives from build-release/ (run after a Release build).

One archive per platform, laid out from the game's install folder:
    OblivionRemastered/Binaries/<Win64|WinGDK>/...
That layout installs as-is when extracted over the game, is a "root mod" for Vortex's Oblivion Remastered
extension, and is moved into Root/ by Mod Organizer 2's game plugin (for Root Builder). The docs sit next to
the DLLs with an OblivionQuickLoot_ prefix, so nothing is deployed loose into the game's root folder and the
names can't collide with other mods' READMEs.
"""
import pathlib
import re
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
version = re.search(r'#define QL_VERSION "([^"]+)"', (ROOT / "src" / "dllmain.cpp").read_text()).group(1)

FILES = {  # archive name -> source
    "dsound.dll": ROOT / "build-release" / "dsound.dll",
    "OblivionQuickLoot.dll": ROOT / "build-release" / "OblivionQuickLoot.dll",
    "OblivionQuickLoot.ini": ROOT / "OblivionQuickLoot.ini",
    "OblivionQuickLoot_README.md": ROOT / "README.md",
    "OblivionQuickLoot_LICENSE.txt": ROOT / "LICENSE",
    "OblivionQuickLoot_THIRD_PARTY_NOTICES.txt": ROOT / "THIRD_PARTY_NOTICES.txt",
}
PLATFORMS = {"Steam": "OblivionRemastered/Binaries/Win64", "GamePass": "OblivionRemastered/Binaries/WinGDK"}

for src in FILES.values():
    if not src.exists():
        raise SystemExit(f"missing {src} - build Release first")

dist = ROOT / "dist"
dist.mkdir(exist_ok=True)
for old in dist.glob("OblivionQuickLoot-*.zip"):
    old.unlink()
for platform, folder in PLATFORMS.items():
    out = dist / f"OblivionQuickLoot-{version}-{platform}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, src in FILES.items():
            z.write(src, f"{folder}/{name}")
    print(out)
    for info in zipfile.ZipFile(out).infolist():
        print(f"  {info.file_size:>9}  {info.filename}")
