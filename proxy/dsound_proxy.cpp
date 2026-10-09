// Loader: a dsound.dll that forwards every export to the real System32 dsound.dll and loads
// OblivionQuickLoot.dll from the same folder.
//
// Why dsound: both the Steam (Win64) and Game Pass (WinGDK) executables import DSOUND.dll, it is not a
// KnownDLL, and no framework package supplies it, so Windows loads it from the game folder on both. (On Game
// Pass, XINPUT1_3 comes from the DirectX Runtime framework package before the game folder is searched.) It
// also doesn't collide with UE4SS (dwmapi.dll) or ReShade (dxgi.dll).
#include <windows.h>

#include "dsound_exports.h"

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
        while (n && path[n - 1] != L'\\') --n;
        path[n] = 0;
        lstrcatW(path, L"OblivionQuickLoot.dll");
        LoadLibraryW(path);  // the mod defers its real work to its own thread
    }
    return TRUE;
}
