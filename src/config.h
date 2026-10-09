#pragma once
#include <string>
#include <vector>

namespace ql {
struct Config {
    bool enabled = true;
    float scale = 1.0f;         // text/panel scale on top of the 1080p-relative scale
    float posX = 0.56f;         // panel left edge, fraction of screen width
    float posY = 0.50f;         // panel vertical centre, fraction of screen height
    int maxRows = 7;
    bool showEmpty = true;      // show "Empty" for empty containers (activate then opens normally)
    bool debugLog = false;
    float fontScale = 1.0f;
    std::wstring titleTypeface = L"";      // composite-font typeface names; empty = font default
    std::wstring bodyTypeface = L"";
    std::wstring activateKeyLabel = L"E";  // shown on the keycap; the game's Activate binding is used either way
    bool gallery = false;      // dev: draw a grid of candidate UI textures/fonts
    int galleryPage = 0;

    // UE FKey names (see UE EKeys). Multiple keys separated by commas.
    std::vector<std::wstring> keysUp{L"MouseScrollUp", L"Up", L"Gamepad_DPad_Up"};
    std::vector<std::wstring> keysDown{L"MouseScrollDown", L"Down", L"Gamepad_DPad_Down"};
    std::vector<std::wstring> keysActivate{L"E", L"Gamepad_FaceButton_Bottom"};  // take the selected item
    std::vector<std::wstring> keysTakeAll{L"T", L"Gamepad_FaceButton_Top"};
    std::vector<std::wstring> keysOpen{L"R", L"Gamepad_FaceButton_Left"};  // open the full container menu
};

extern Config g_config;

// Diagnostic logging, only with [General] bDebugLog=1. Errors and faults always use QL_LOG.
#define QL_DEBUG(...)                                       do {                                                        if (::ql::g_config.debugLog) QL_LOG(__VA_ARGS__);     } while (0)
void LoadConfig(const std::wstring& iniPath);
}  // namespace ql
