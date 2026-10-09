#include "assets.h"

#include <cwctype>
#include <unordered_map>

#include "config.h"
#include "log.h"

namespace ql::assets {
using namespace game;

namespace {
std::unordered_map<std::wstring, Texture> g_textures;
std::unordered_map<std::wstring, UFont*> g_fonts;

// Root everything we load (UObject::AddToRoot) so GC never frees a texture the canvas is still drawing.
// An FGCObject referencer was tried first, but it relies on FReferenceCollector's vtable layout from the
// reference build; rooting uses the same flag sequence the engine itself uses in this build.
void Keep(UObject* obj) { AddToRoot(obj); }

FString Str(const std::wstring& s)
{
    return FString{s.c_str(), static_cast<int32_t>(s.size() + 1), static_cast<int32_t>(s.size() + 1)};
}

UObject* Load(void* cls, const std::wstring& packagePath)
{
    FString pkg = Str(packagePath);
    if (!g_fn.DoesPackageExist(&pkg, nullptr, false)) return nullptr;
    std::wstring objectPath = packagePath + L"." + packagePath.substr(packagePath.find_last_of(L'/') + 1);
    UObject* obj = g_fn.StaticLoadObject(cls, nullptr, objectPath.c_str(), nullptr, 0, nullptr, true, nullptr);
    if (obj) Keep(obj);
    return obj;
}
}  // namespace

Texture GetTexture(const std::wstring& packagePath)
{
    auto it = g_textures.find(packagePath);
    if (it != g_textures.end()) return it->second;
    Texture t;
    t.object = Load(g_fn.UTexture2D_StaticClass(), packagePath);
    if (t.object) {
        t.width = g_fn.UTexture2D_GetSizeX(t.object);
        t.height = g_fn.UTexture2D_GetSizeY(t.object);
    }
    if (g_config.debugLog) QL_LOG("texture %ls -> %p (%dx%d)", packagePath.c_str(), t.object, t.width, t.height);
    g_textures.emplace(packagePath, t);
    return t;
}

UFont* GetFont(const std::wstring& packagePath)
{
    auto it = g_fonts.find(packagePath);
    if (it != g_fonts.end()) return it->second;
    auto font = reinterpret_cast<UFont*>(Load(g_fn.UFont_StaticClass(), packagePath));
    QL_DEBUG("font %ls -> %p", packagePath.c_str(), font);
    g_fonts.emplace(packagePath, font);
    return font;
}

Texture GetItemIcon(const std::wstring& legacy)
{
    if (legacy.empty()) return {};
    std::wstring p = legacy;
    for (auto& c : p)
        if (c == L'\\') c = L'/';
    size_t dot = p.find_last_of(L'.');
    if (dot != std::wstring::npos) p.resize(dot);
    size_t slash = p.find_last_of(L'/');
    std::wstring dir = slash == std::wstring::npos ? L"" : p.substr(0, slash + 1);
    std::wstring stem = slash == std::wstring::npos ? p : p.substr(slash + 1);
    // Some records include the "menus/icons/" prefix; strip it so both forms map the same way.
    std::wstring lower = dir;
    for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
    if (lower.rfind(L"menus/icons/", 0) == 0) dir = dir.substr(12);
    return GetTexture(L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/" + dir + L"T_" + stem);
}
}  // namespace ql::assets
