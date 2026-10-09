#include "draw.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "log.h"

namespace ql {
using namespace game;

namespace {
FString Str(const std::wstring& s)
{
    return FString{s.c_str(), static_cast<int32_t>(s.size() + 1), static_cast<int32_t>(s.size() + 1)};
}

float CanvasFloat(UCanvas* c, size_t off) { return *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(c) + off); }

void SetBlend(FCanvasTileItemStorage& item) { *reinterpret_cast<uint32_t*>(item.bytes + kCanvasItem_BlendMode) = SE_BLEND_Translucent; }

float SrgbToLinear(uint8_t c)
{
    float v = c / 255.0f;
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

FLinearColor ToLinear(Color8 c) { return {SrgbToLinear(c.r), SrgbToLinear(c.g), SrgbToLinear(c.b), c.a / 255.0f}; }

// FTexts handed to FCanvasTextItem. Created once per distinct string and kept for the session (the set is
// small: item names, counts and a few labels), so no FText ever has to be released by hand.
const FText* CachedText(const std::wstring& s)
{
    static std::unordered_map<std::wstring, FText> cache;
    auto it = cache.find(s);
    if (it != cache.end()) return &it->second;
    FText t{};
    FString fs = Str(s);
    g_fn.FText_AsCultureInvariant(&t, &fs);  // copies the string into engine memory
    return &cache.emplace(s, t).first->second;
}

struct MeasureKey {
    std::wstring text;
    UFont* font;
    float size;
    uint32_t typeface;
    bool operator<(const MeasureKey& o) const
    {
        return std::tie(font, size, typeface, text) < std::tie(o.font, o.size, o.typeface, o.text);
    }
};
std::map<MeasureKey, FVector2D> g_measured;
}  // namespace

// The debug canvas draws in DPI-scaled units: SizeX/Y are physical pixels (3840x2160) but the visible
// area is ClipX/ClipY (2560x1440 at 150% UI scale). Lay out against Clip.
Canvas::Canvas(UCanvas* c) : c_(c), w_(CanvasFloat(c, kUCanvas_ClipX)), h_(CanvasFloat(c, kUCanvas_ClipY)) {}

UFont* Canvas::EngineFont() const { return g_fn.UEngine_GetMediumFont(); }

void Canvas::Rect(float x, float y, float w, float h, const FLinearColor& color) const
{
    FCanvasTileItemStorage item;
    FVector2D pos{x, y}, size{w, h};
    g_fn.FCanvasTileItem_ctor(&item, &pos, &size, &color);
    SetBlend(item);
    g_fn.UCanvas_DrawItem(c_, &item);
}

void Canvas::Image(const assets::Texture& t, float x, float y, float w, float h, const FLinearColor& tint, float u0,
                   float v0, float u1, float v1) const
{
    if (!t || w <= 0 || h <= 0) return;
    // A freshly loaded texture has its FTextureResource before the render thread has created the RHI texture;
    // drawing it then hands the RHI thread a null texture (crash in RHISetShaderParameters). Wait for
    // FTexture::TextureRHI (+0x10) and SamplerStateRHI (+0x18).
    auto resource = static_cast<uint8_t*>(g_fn.UTexture_GetResource(t.object));
    if (!resource || !*reinterpret_cast<void**>(resource + 0x10) || !*reinterpret_cast<void**>(resource + 0x18)) return;
    FCanvasTileItemStorage item;
    FVector2D pos{x, y}, size{w, h};
    g_fn.FCanvasTileItem_ctorTex(&item, &pos, resource, &size, &tint);
    *reinterpret_cast<FVector2D*>(item.bytes + kTileItem_UV0) = {u0, v0};
    *reinterpret_cast<FVector2D*>(item.bytes + kTileItem_UV1) = {u1, v1};
    SetBlend(item);
    g_fn.UCanvas_DrawItem(c_, &item);
}

FVector2D Canvas::DrawTextItem(const TextStyle& st, const std::wstring& s, float x, float y, const FLinearColor& color,
                               bool shadow) const
{
    FCanvasTextItemStorage item;
    FVector2D pos{x, y};
    g_fn.FCanvasTextItem_ctor(&item, &pos, CachedText(s), st.font, &color);

    uint8_t outline[0x20] = {};  // FFontOutlineSettings: no outline
    g_fn.FSlateFontInfo_ctor(item.bytes + kTextItem_SlateFontInfo, reinterpret_cast<UObject*>(st.font), st.size,
                             &st.typeface, outline);
    item.bytes[kTextItem_SlateFontInfoIsSet] = 1;

    if (shadow) {
        *reinterpret_cast<uint32_t*>(item.bytes + kTextItem_FontRenderInfo) |= 0x2;  // bEnableShadow
        *reinterpret_cast<FLinearColor*>(item.bytes + kTextItem_ShadowColor) = {0, 0, 0, color.a * 0.75f};
        *reinterpret_cast<FVector2D*>(item.bytes + kTextItem_ShadowOffset) = {1.0, 1.0};
    }
    g_fn.UCanvas_DrawItem(c_, &item);
    FVector2D drawn = *reinterpret_cast<FVector2D*>(item.bytes + kTextItem_DrawnSize);
    g_fn.FCanvasTextItem_dtor(&item);
    return drawn;
}

void Canvas::Text(const TextStyle& st, const std::wstring& s, float x, float y, Color8 color, bool shadow) const
{
    if (!st.font || s.empty()) return;
    FVector2D drawn = DrawTextItem(st, s, x, y, ToLinear(color), shadow);
    g_measured[{s, st.font, st.size, st.typeface.index}] = drawn;
}

float Canvas::TextWidth(const TextStyle& st, const std::wstring& s) const
{
    if (!st.font || s.empty()) return 0;
    auto it = g_measured.find({s, st.font, st.size, st.typeface.index});
    if (it != g_measured.end()) return static_cast<float>(it->second.x);
    // Measure by drawing fully transparent, far off-screen; the item reports its DrawnSize.
    FVector2D drawn = DrawTextItem(st, s, -20000.0f, -20000.0f, {0, 0, 0, 0}, false);
    g_measured[{s, st.font, st.size, st.typeface.index}] = drawn;
    return static_cast<float>(drawn.x);
}

float Canvas::TextHeight(const TextStyle& st) const
{
    if (!st.font) return 0;
    TextWidth(st, L"Ag");
    auto it = g_measured.find({L"Ag", st.font, st.size, st.typeface.index});
    return it != g_measured.end() ? static_cast<float>(it->second.y) : 0;
}

std::wstring Canvas::Ellipsize(const TextStyle& st, std::wstring s, float maxW) const
{
    if (TextWidth(st, s) <= maxW) return s;
    while (s.size() > 1 && TextWidth(st, s + L"…") > maxW) s.pop_back();
    return s + L"…";
}

void Canvas::LegacyText(UFont* font, const std::wstring& s, float x, float y, float scale, Color8 col) const
{
    if (!font || s.empty()) return;
    FString fs = Str(s);
    FFontRenderInfo info{};
    uint8_t* p = reinterpret_cast<uint8_t*>(c_) + kUCanvas_DrawColor;  // FColor is stored B,G,R,A
    p[0] = col.b; p[1] = col.g; p[2] = col.r; p[3] = col.a;
    g_fn.UCanvas_DrawText(c_, font, &fs, x, y, scale, scale, &info);
}

const std::wstring& Localize(const std::wstring& key)
{
    static std::unordered_map<std::wstring, std::wstring> cache;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    std::wstring out = key;
    if (key.rfind(L"LOC_", 0) == 0 || key.rfind(L"UI_", 0) == 0) {
        std::string narrow;  // keys are ASCII
        for (wchar_t ch : key) narrow.push_back(static_cast<char>(ch));
        FText text{};
        g_fn.LocalizationManager_GetFTextFromKey(&text, narrow.c_str());
        // `text` is intentionally not released: its destructor is inlined everywhere in the engine, and we
        // resolve each key once per session.
        if (const FString* s = g_fn.FText_ToString(&text); s && s->data && s->num > 1)
            out.assign(s->data, static_cast<size_t>(s->num - 1));
        if (g_config.debugLog) QL_LOG("localize %ls -> %ls", key.c_str(), out.c_str());
    }
    return cache.emplace(key, std::move(out)).first->second;
}

void LogTypefaces(UFont* font, const wchar_t* label)
{
    if (!font) return;
    // UFont::CompositeFont (+0x148) -> DefaultTypeface.Fonts : TArray<FTypefaceEntry> (stride 0x30, FName first)
    auto base = reinterpret_cast<uint8_t*>(font) + 0x148;
    auto entries = *reinterpret_cast<uint8_t**>(base);
    int count = *reinterpret_cast<int32_t*>(base + 8);
    for (int i = 0; i < count && i < 16; ++i) {
        FString name{};
        g_fn.FName_ToString(reinterpret_cast<FName*>(entries + i * 0x30), &name);
        QL_LOG("font %ls typeface[%d] = %ls", label, i, name.data ? name.data : L"?");
    }
}
}  // namespace ql
