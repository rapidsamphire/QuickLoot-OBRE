#pragma once
// Loads the remaster's own UI assets (UTexture2D / UFont) by package path and keeps them alive with an
// FGCObject. UE game thread only.
#include <string>

#include "game.h"

namespace ql::assets {

struct Texture {
    game::UObject* object = nullptr;
    int width = 0, height = 0;
    explicit operator bool() const { return object != nullptr; }
};

// "/Game/Art/UI/Common/T_List_highlight_V1" (package path, no ".Object" suffix). Cached, nullptr if missing.
Texture GetTexture(const std::wstring& packagePath);
game::UFont* GetFont(const std::wstring& packagePath);

// Legacy TES icon path ("Armor\Iron\M\Cuirass.dds") -> modern icon texture
// (/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/armor/Iron/M/T_cuirass).
Texture GetItemIcon(const std::wstring& legacyIconPath);

}  // namespace ql::assets
