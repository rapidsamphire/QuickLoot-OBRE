"""Build the release archive from build-release/ (run after a Release build).

Layout: the docs at the top, plus Steam/ and GamePass/ folders laid out from the game's install folder,
so either can be extracted straight over the game.
"""
import pathlib
import re
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
version = re.search(r'#define QL_VERSION "([^"]+)"', (ROOT / "src" / "dllmain.cpp").read_text()).group(1)

BINARIES = [ROOT / "build-release" / "dsound.dll", ROOT / "build-release" / "OblivionQuickLoot.dll",
            ROOT / "OblivionQuickLoot.ini"]
DOCS = ["README.md", "LICENSE", "THIRD_PARTY_NOTICES.txt"]
TARGETS = {"Steam": "OblivionRemastered/Binaries/Win64", "GamePass": "OblivionRemastered/Binaries/WinGDK"}

out = ROOT / "dist" / f"OblivionQuickLoot-{version}.zip"
out.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for doc in DOCS:
        z.write(ROOT / doc, doc)
    for store, folder in TARGETS.items():
        for f in BINARIES:
            if not f.exists():
                raise SystemExit(f"missing {f} - build Release first")
            z.write(f, f"{store}/{folder}/{f.name}")
print(out)
for info in zipfile.ZipFile(out).infolist():
    print(f"  {info.file_size:>9}  {info.filename}")
