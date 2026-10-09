#pragma once
// Thin wrapper over the engine's UCanvas so panel code reads like drawing code.
#include <string>

#include "assets.h"
#include "game.h"

namespace ql {

struct Color8 { uint8_t r, g, b, a; };

// A Slate font: the game's composite UFont at an exact point size and typeface, rendered through
// FCanvasTextItem's FSlateFontInfo path (the same font cache UMG uses), not the legacy scaled-bitmap path.
struct TextStyle {
    game::UFont* font = nullptr;
    float size = 18.0f;        // Slate font size (points)
    game::FName typeface{};    // e.g. "Regular" / "Medium" / "Bold"; none = the font's default typeface
};

class Canvas {
public:
    explicit Canvas(game::UCanvas* c);

    float Width() const { return w_; }
    float Height() const { return h_; }
    float Scale() const { return h_ / 1080.0f; }  // 1080p-relative UI scale

    game::UFont* EngineFont() const;

    void Rect(float x, float y, float w, float h, const game::FLinearColor& color) const;
    // Draws a texture (translucent). uv0/uv1 select a sub-rectangle in 0..1 texture space.
    void Image(const assets::Texture& t, float x, float y, float w, float h,
               const game::FLinearColor& tint = {1, 1, 1, 1}, float u0 = 0, float v0 = 0, float u1 = 1,
               float v1 = 1) const;

    void Text(const TextStyle& style, const std::wstring& s, float x, float y, Color8 color, bool shadow = false) const;
    float TextWidth(const TextStyle& style, const std::wstring& s) const;
    float TextHeight(const TextStyle& style) const;
    std::wstring Ellipsize(const TextStyle& style, std::wstring s, float maxW) const;

    // Legacy UFont path (engine fonts only; used by the dev gallery labels).
    void LegacyText(game::UFont* font, const std::wstring& s, float x, float y, float scale, Color8 color) const;

private:
    game::FVector2D DrawTextItem(const TextStyle& style, const std::wstring& s, float x, float y,
                                 const game::FLinearColor& color, bool shadow) const;

    game::UCanvas* c_;
    float w_, h_;
};

// Resolves the remaster's localization keys ("LOC_FN_IronCuirass") to display text in the current
// language via LocalizationManager::GetFTextFromKey, as the game's own menus do. Plain strings pass
// through. UE game thread; cached.
const std::wstring& Localize(const std::wstring& keyOrText);

// Dev: logs a composite font's typeface names once.
void LogTypefaces(game::UFont* font, const wchar_t* label);

}  // namespace ql
