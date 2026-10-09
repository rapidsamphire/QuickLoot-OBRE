#include "config.h"

#include <windows.h>

namespace ql {
Config g_config;

namespace {
std::wstring ReadStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, const std::wstring& def)
{
    wchar_t buf[512];
    GetPrivateProfileStringW(sec, key, def.c_str(), buf, 512, ini.c_str());
    return buf;
}

float ReadFloat(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, float def)
{
    std::wstring s = ReadStr(ini, sec, key, L"");
    return s.empty() ? def : static_cast<float>(_wtof(s.c_str()));
}

std::wstring Join(const std::vector<std::wstring>& v)
{
    std::wstring out;
    for (auto& s : v) out += (out.empty() ? L"" : L",") + s;
    return out;
}

std::vector<std::wstring> ReadKeys(const std::wstring& ini, const wchar_t* key, const std::vector<std::wstring>& def)
{
    std::wstring s = ReadStr(ini, L"Keys", key, Join(def));
    std::vector<std::wstring> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t comma = s.find(L',', start);
        std::wstring tok = s.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
        tok.erase(0, tok.find_first_not_of(L" \t"));
        tok.erase(tok.find_last_not_of(L" \t") + 1);
        if (!tok.empty()) out.push_back(tok);
        if (comma == std::wstring::npos) break;
        start = comma + 1;
    }
    return out;
}
}  // namespace

void LoadConfig(const std::wstring& ini)
{
    Config& c = g_config;
    c.enabled = GetPrivateProfileIntW(L"General", L"bEnabled", c.enabled, ini.c_str()) != 0;
    c.scale = ReadFloat(ini, L"General", L"fScale", c.scale);
    c.posX = ReadFloat(ini, L"General", L"fPosX", c.posX);
    c.posY = ReadFloat(ini, L"General", L"fPosY", c.posY);
    c.maxRows = GetPrivateProfileIntW(L"General", L"iMaxRows", c.maxRows, ini.c_str());
    c.showEmpty = GetPrivateProfileIntW(L"General", L"bShowEmpty", c.showEmpty, ini.c_str()) != 0;
    c.debugLog = GetPrivateProfileIntW(L"General", L"bDebugLog", c.debugLog, ini.c_str()) != 0;
    c.gallery = GetPrivateProfileIntW(L"Dev", L"bGallery", c.gallery, ini.c_str()) != 0;
    c.galleryPage = GetPrivateProfileIntW(L"Dev", L"iGalleryPage", c.galleryPage, ini.c_str());
    c.activateKeyLabel = ReadStr(ini, L"Keys", L"ActivateLabel", c.activateKeyLabel);
    c.fontScale = ReadFloat(ini, L"General", L"fFontScale", c.fontScale);
    c.titleTypeface = ReadStr(ini, L"Fonts", L"TitleTypeface", c.titleTypeface);
    c.bodyTypeface = ReadStr(ini, L"Fonts", L"BodyTypeface", c.bodyTypeface);
    c.keysUp = ReadKeys(ini, L"ScrollUp", c.keysUp);
    c.keysDown = ReadKeys(ini, L"ScrollDown", c.keysDown);
    c.keysTakeAll = ReadKeys(ini, L"TakeAll", c.keysTakeAll);
    c.keysActivate = ReadKeys(ini, L"Activate", c.keysActivate);
    c.keysOpen = ReadKeys(ini, L"Open", c.keysOpen);
    if (c.maxRows < 1) c.maxRows = 1;
    if (c.maxRows > 20) c.maxRows = 20;
}
}  // namespace ql
