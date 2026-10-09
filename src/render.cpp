// The Quick Loot panel, drawn with the remaster's own modern-UI assets: the HUD tutorial backing and gold
// border lines, Kingthings/Robinson fonts, the list highlight strip, inventory separator, item icons from
// /Game/Art/UI/Icons/Dynamic_Icons and the PC keycap / Xbox button glyphs used by the game's own prompts.
#include "render.h"

#include <algorithm>
#include <string>

#include "assets.h"
#include "config.h"
#include "draw.h"
#include "quickloot.h"

namespace ql {
using namespace game;

namespace {
namespace path {
constexpr const wchar_t* Backing = L"/Game/Art/UI/Common/Backgrounds/T_UI_HUD_TutorialBackground";
constexpr const wchar_t* Border = L"/Game/Art/UI/Common/Backgrounds/T_UI_HUD_TutorialBorder";
constexpr const wchar_t* Highlight = L"/Game/Art/UI/Common/T_List_highlight_V1";
constexpr const wchar_t* RowShade = L"/Game/Art/UI/Common/T_List_highlight_V2";
constexpr const wchar_t* Separator = L"/Game/Art/UI/Common/T_InventorySeparator";
constexpr const wchar_t* Stolen = L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_Icon_stolen";
constexpr const wchar_t* Value = L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_icon_small_value";
constexpr const wchar_t* Weight = L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_icon_small_weight";
constexpr const wchar_t* KeyL = L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Left";
constexpr const wchar_t* KeyM = L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Middle";
constexpr const wchar_t* KeyR = L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Right";
constexpr const wchar_t* PadA = L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_A";
constexpr const wchar_t* PadX = L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_X";
constexpr const wchar_t* PadY = L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_Y";
constexpr const wchar_t* FontTitle = L"/Game/UI/CommonStyle/Fonts/Font_Kingthings_Localized";
constexpr const wchar_t* FontBody = L"/Game/UI/CommonStyle/Fonts/Font_Robinson_Localized";
}  // namespace path

// Palette sampled from the remaster's menus (parchment text on dark, warm gold accents).
constexpr Color8 kTitle{236, 224, 196, 255};
constexpr Color8 kText{222, 212, 190, 255};
constexpr Color8 kTextDim{160, 150, 132, 255};
constexpr Color8 kTextOnParchment{48, 36, 24, 255};
constexpr Color8 kTextStolen{214, 96, 78, 255};
constexpr Color8 kKeyLabel{230, 220, 200, 255};
constexpr FLinearColor kGold{0.95f, 0.80f, 0.52f, 1.0f};
constexpr FLinearColor kOrnament{0.80f, 0.70f, 0.52f, 0.85f};

bool g_gamepad = false;

struct Styles {
    TextStyle title, body, small, key, label;
};

// Sizes are in 1080p pixels, converted to Slate points (pt = px * 0.75) and scaled with the screen.
Styles MakeStyles(const Canvas& c, float s)
{
    UFont* king = assets::GetFont(path::FontTitle);
    UFont* robin = assets::GetFont(path::FontBody);
    static bool logged = false;
    if (!logged && g_config.debugLog) {
        logged = true;
        LogTypefaces(king, L"Kingthings");
        LogTypefaces(robin, L"Robinson");
    }
    if (!king) king = c.EngineFont();
    if (!robin) robin = c.EngineFont();
    static FName regular = MakeName(g_config.bodyTypeface.c_str());
    static FName titleFace = MakeName(g_config.titleTypeface.c_str());
    auto pt = [&](float px) { return px * 0.75f * s * g_config.fontScale; };
    return Styles{
        TextStyle{king, pt(30), titleFace},
        TextStyle{robin, pt(21), regular},
        TextStyle{robin, pt(17), regular},
        TextStyle{robin, pt(17), regular},
        TextStyle{king, pt(24), titleFace},
    };
}

// A keyboard keycap built from the game's three-piece input background, like the menus' footer prompts.
float DrawKeycap(const Canvas& c, const TextStyle& font, const std::wstring& key, float x, float y, float h)
{
    assets::Texture l = assets::GetTexture(path::KeyL), m = assets::GetTexture(path::KeyM),
                    r = assets::GetTexture(path::KeyR);
    const float capW = h * 0.5f;  // pieces are 16x32
    const float textW = c.TextWidth(font, key);
    const float midW = std::max(h * 0.25f, textW + h * 0.1f - 2 * capW + h * 0.5f);
    c.Image(l, x, y, capW, h);
    c.Image(m, x + capW, y, midW, h);
    c.Image(r, x + capW + midW, y, capW, h);
    const float totalW = 2 * capW + midW;
    const float th = c.TextHeight(font);
    c.Text(font, key, x + (totalW - textW) * 0.5f, y + (h - th) * 0.5f, kKeyLabel);
    return totalW;
}

float DrawPrompt(const Canvas& c, const Styles& f, float s, const std::wstring& key, const wchar_t* padIcon,
                 const std::wstring& label, float x, float y)
{
    const float h = 30 * s;
    float w;
    if (g_gamepad && padIcon) {
        c.Image(assets::GetTexture(padIcon), x, y - 2 * s, h + 4 * s, h + 4 * s);
        w = h + 4 * s;
    } else {
        w = DrawKeycap(c, f.key, key, x, y, h);
    }
    c.Text(f.label, label, x + w + 8 * s, y + (h - c.TextHeight(f.label)) * 0.5f, kText, true);
    return w + 8 * s + c.TextWidth(f.label, label);
}

std::wstring FirstKeyboardKey(const std::vector<std::wstring>& keys, const wchar_t* fallback)
{
    for (auto& k : keys)
        if (k.rfind(L"Gamepad", 0) != 0 && k.rfind(L"Mouse", 0) != 0) return k;
    return fallback;
}

std::wstring FormatWeight(float w)
{
    wchar_t buf[32];
    swprintf_s(buf, w == static_cast<int>(w) ? L"%.0f" : L"%.1f", w);
    return buf;
}
}  // namespace

void SetGamepadActive(bool gamepad) { g_gamepad = gamepad; }

void DrawPanel(UCanvas* canvas)
{
    if (!g_config.enabled || !canvas || !PanelActive(false)) return;

    std::wstring title;
    std::vector<LootItem> items;
    int selected;
    bool stealing;
    {
        std::lock_guard lock(g_shared.mutex);
        const LootSnapshot& snap = g_shared.snapshot;
        if (g_shared.selectedRef != snap.ref) {
            g_shared.selectedRef = snap.ref;
            g_shared.selected = 0;
        }
        if (!snap.items.empty())
            g_shared.selected = std::clamp(g_shared.selected, 0, static_cast<int>(snap.items.size()) - 1);
        title = snap.title;
        items = snap.items;
        selected = g_shared.selected;
        stealing = snap.stealing;
    }

    Canvas c(canvas);
    if (c.Width() <= 0 || c.Height() <= 0) return;
    const float s = c.Scale() * g_config.scale;
    const Styles f = MakeStyles(c, s);

    const float width = 470 * s, padX = 20 * s;
    const float titleH = 46 * s, sepH = 14 * s, rowH = 40 * s, infoH = items.empty() ? 0 : 34 * s;
    const int rows = std::min<int>(g_config.maxRows, std::max<int>(1, static_cast<int>(items.size())));
    const float height = titleH + sepH + rows * rowH + infoH + 14 * s;
    const float x = c.Width() * g_config.posX;
    const float y = c.Height() * g_config.posY - height * 0.5f;

    // Backing: the HUD tutorial gradient (opaque left, fading right), as the game's own HUD popups use.
    c.Image(assets::GetTexture(path::Backing), x, y, width * 1.15f, height, {1, 1, 1, 0.92f}, 0.0f, 0.0f, 0.82f, 1.0f);
    assets::Texture border = assets::GetTexture(path::Border);
    c.Image(border, x, y - 2 * s, width, 3 * s, kGold);
    c.Image(border, x, y + height - 1 * s, width, 3 * s, kGold);

    // Title + ornament separator
    const std::wstring heading = title.empty() ? L"Container" : Localize(title);
    const float stealW = stealing ? 34 * s : 0;
    // Centre the title between the box's top edge and the divider's line (the ornament texture is drawn
    // centred in its sepH-high strip), so the gap above the text equals the gap below it.
    const float sepY = y + titleH - 2 * s;
    const float dividerY = sepY + sepH * 0.5f;
    const float titleTextH = c.TextHeight(f.title);
    // Kingthings' line box has tall ascender space: its glyphs sit ~0.36 x font size above the box centre
    // (measured in-game), so shift down by that to centre the visible letters, not the line box.
    const float titleY = y + (dividerY - y - titleTextH) * 0.5f + 0.36f * f.title.size;
    c.Text(f.title, c.Ellipsize(f.title, heading, width - 2 * padX - stealW), x + padX, titleY, kTitle, true);
    if (stealing)
        c.Image(assets::GetTexture(path::Stolen), x + width - padX - 26 * s, y + (dividerY - y - 26 * s) * 0.5f, 26 * s, 26 * s);
    c.Image(assets::GetTexture(path::Separator), x + padX, sepY, width - 2 * padX, sepH, kOrnament);

    float rowY = y + titleH + sepH;
    if (items.empty()) {
        c.Text(f.body, Localize(L"Empty"), x + padX, rowY + (rowH - c.TextHeight(f.body)) * 0.5f, kTextDim);
    } else {
        const int first = std::clamp(selected - rows / 2, 0, std::max(0, static_cast<int>(items.size()) - rows));
        const float iconSize = 30 * s;
        for (int i = first; i < first + rows && i < static_cast<int>(items.size()); ++i, rowY += rowH) {
            const LootItem& it = items[static_cast<size_t>(i)];
            const bool sel = i == selected;
            if (sel)
                c.Image(assets::GetTexture(path::Highlight), x + 6 * s, rowY + 2 * s, width - 12 * s, rowH - 4 * s);
            else if (i % 2 == 1)
                c.Image(assets::GetTexture(path::RowShade), x + 6 * s, rowY + 3 * s, width - 12 * s, rowH - 6 * s,
                        {0.6f, 0.55f, 0.45f, 0.06f});

            float textX = x + padX;
            if (assets::Texture icon = assets::GetItemIcon(it.icon)) {
                c.Image(icon, textX, rowY + (rowH - iconSize) * 0.5f, iconSize, iconSize);
                textX += iconSize + 10 * s;
            }

            std::wstring right = it.count > 1 ? L"×" + std::to_wstring(it.count) : L"";
            const float rightW = right.empty() ? 0 : c.TextWidth(f.body, right);
            const float stolenW = it.stolen ? 24 * s : 0;
            const Color8 col = sel ? (it.stolen ? Color8{150, 40, 30, 255} : kTextOnParchment)
                                   : (it.stolen ? kTextStolen : kText);
            const float ty = rowY + (rowH - c.TextHeight(f.body)) * 0.5f;
            const float nameMax = x + width - padX - rightW - stolenW - 8 * s - textX;
            c.Text(f.body, c.Ellipsize(f.body, Localize(it.name), nameMax), textX, ty, col, !sel);
            if (!right.empty())
                c.Text(f.body, right, x + width - padX - rightW, ty, sel ? kTextOnParchment : kTextDim, !sel);
            if (it.stolen)
                c.Image(assets::GetTexture(path::Stolen), x + width - padX - rightW - stolenW - 4 * s,
                        rowY + (rowH - 20 * s) * 0.5f, 20 * s, 20 * s);
        }

        // Scroll position: a thin gold track on the right edge when the list overflows.
        if (static_cast<int>(items.size()) > rows) {
            const float trackY = y + titleH + sepH, trackH = rows * rowH;
            const float thumbH = trackH * rows / items.size();
            const float thumbY = trackY + (trackH - thumbH) * first / (items.size() - rows);
            c.Rect(x + width - 5 * s, trackY, 2 * s, trackH, {0.5f, 0.42f, 0.3f, 0.35f});
            c.Rect(x + width - 6 * s, thumbY, 4 * s, thumbH, {0.95f, 0.8f, 0.52f, 0.9f});
        }

        // Selected item's value and weight, with the game's stat icons.
        const LootItem& cur = items[static_cast<size_t>(selected)];
        const float iy = y + titleH + sepH + rows * rowH + 6 * s, ih = 22 * s;
        float ix = x + padX;
        c.Image(assets::GetTexture(path::Value), ix, iy, ih, ih);
        ix += ih + 6 * s;
        std::wstring v = std::to_wstring(cur.value);
        c.Text(f.small, v, ix, iy + (ih - c.TextHeight(f.small)) * 0.5f, kTextDim);
        ix += c.TextWidth(f.small, v) + 22 * s;
        c.Image(assets::GetTexture(path::Weight), ix, iy, ih, ih);
        ix += ih + 6 * s;
        c.Text(f.small, FormatWeight(cur.weight), ix, iy + (ih - c.TextHeight(f.small)) * 0.5f, kTextDim);
    }

    // Prompts under the panel, in the game's footer style.
    const std::wstring activate = FirstKeyboardKey(g_config.keysActivate, L"E");
    const std::wstring open = FirstKeyboardKey(g_config.keysOpen, L"R");
    float px = x + padX, py = y + height + 14 * s;
    if (!items.empty()) {
        px += DrawPrompt(c, f, s, activate, path::PadA, L"Take", px, py) + 26 * s;
        px += DrawPrompt(c, f, s, FirstKeyboardKey(g_config.keysTakeAll, L"T"), path::PadY, L"Take All", px, py) + 26 * s;
    }
    DrawPrompt(c, f, s, open, path::PadX, L"Search", px, py);
}
}  // namespace ql
