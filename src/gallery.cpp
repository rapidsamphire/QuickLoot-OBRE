// Dev aid: draws the remaster's candidate UI textures and fonts in a labelled grid so they can be
// inspected in-game (we can't ship or extract the assets, but we can look at them).
#include <string>
#include <vector>

#include "assets.h"
#include "config.h"
#include "draw.h"

namespace ql {
using namespace game;

namespace {
const std::vector<const wchar_t*> kPages[] = {
    {
        L"/Game/Art/UI/Common/Backgrounds/T_UI_HUD_TutorialBackground",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_HUD_TutorialBorder",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_HUD_NotificationsBackground_QHD",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_EntrySelection",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_BackgroundHorizontal",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_MenuLeft",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_MenuRight",
        L"/Game/Art/UI/Common/Backgrounds/T_UI_Clip",
        L"/Game/Art/UI/Common/T_BGGradient",
        L"/Game/Art/UI/Common/T_FooterGradient",
        L"/Game/Art/UI/Common/T_Empty_Background_D",
        L"/Game/Art/UI/Common/T_NotificationMenuEmptyStatesBG",
    },
    {
        L"/Game/Art/UI/Common/T_List_highlight_V1",
        L"/Game/Art/UI/Common/T_List_highlight_V2",
        L"/Game/Art/UI/Common/T_List_highlight_V3",
        L"/Game/Art/UI/Common/T_List_Default",
        L"/Game/Art/UI/Common/T_List_Darkened",
        L"/Game/Art/UI/Common/T_List_equip",
        L"/Game/Art/UI/Common/T_InventorySeparator",
        L"/Game/Art/UI/Common/T_InventorySeparator_Short",
        L"/Game/Art/UI/Common/T_Diamond_Brown_Separator",
        L"/Game/Art/UI/Common/T_Diamond_LightBrown_Separator",
        L"/Game/Art/UI/Common/T_decoration_line",
        L"/Game/Art/UI/Common/T_MenuDeco",
        L"/Game/Art/UI/Common/T_PauseMenuDeco",
        L"/Game/Art/UI/Common/T_Line_glow",
        L"/Game/Art/UI/Common/T_Line_Brown",
        L"/Game/Art/UI/Common/T_Line_02",
    },
    {
        L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Left",
        L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Middle",
        L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Input_Backgound_Right",
        L"/Game/Art/UI/Icons/Icons_Inputs/PC/T_Icon_Mouse",
        L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_A",
        L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_X",
        L"/Game/Art/UI/Icons/Icons_Inputs/Xbox/T_Icon_Xbox_DPad_Up_Down",
        L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_icon_small_weight",
        L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_icon_small_value",
        L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_Icon_stolen",
        L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/T_icon_large_open_container",
        L"/Game/Art/UI/Icons/Icon_Stats/T_Property_1_Weight",
        L"/Game/Art/UI/Icons/Icon_Stats/T_Property_1_MoneyV2",
        L"/Game/Art/UI/Icons/Icon_Stats/T_Property_Weight_White",
        L"/Game/Art/UI/Icons/Icon_Stats/T_Property_Money_White",
        L"/Game/Art/UI/Icons/Dynamic_Icons/menus/Icons/armor/Iron/M/T_cuirass",
    },
};
}  // namespace

void DrawGallery(UCanvas* canvas)
{
    const int page = g_config.galleryPage % static_cast<int>(std::size(kPages));
    Canvas c(canvas);
    const float s = c.Scale();
    UFont* label = c.EngineFont();
    UFont* king = assets::GetFont(L"/Game/UI/CommonStyle/Fonts/Font_Kingthings_Localized");
    UFont* robin = assets::GetFont(L"/Game/UI/CommonStyle/Fonts/Font_Robinson_Localized");

    c.Rect(0, 0, c.Width(), c.Height(), {0.25f, 0.25f, 0.28f, 1.0f});  // neutral backdrop to judge alpha
    const auto& paths = kPages[page];
    const int cols = 4;
    const float cellW = c.Width() / cols, cellH = (c.Height() - 160 * s) / 4;
    for (size_t i = 0; i < paths.size(); ++i) {
        float x = (i % cols) * cellW + 10 * s, y = (i / cols) * cellH + 10 * s;
        assets::Texture t = assets::GetTexture(paths[i]);
        std::wstring name = paths[i];
        name = name.substr(name.find_last_of(L'/') + 1);
        if (t) {
            float bw = cellW - 20 * s, bh = cellH - 50 * s;
            float k = std::min(bw / t.width, bh / t.height);
            c.Image(t, x, y, t.width * k, t.height * k);
            name += L"  " + std::to_wstring(t.width) + L"x" + std::to_wstring(t.height);
        } else {
            name += L"  (missing)";
        }
        c.LegacyText(label, name, x, y + cellH - 40 * s, 0.8f * s, {255, 255, 255, 255});
    }
    float fy = c.Height() - 150 * s;
    if (king) c.Text(TextStyle{king, 30 * s * 0.75f, {}}, L"Kingthings: Iron Cuirass  ×3   Take All", 20 * s, fy, {240, 220, 170, 255});
    if (robin) c.Text(TextStyle{robin, 30 * s * 0.75f, {}}, L"Robinson: Iron Cuirass  ×3   Take All", 20 * s, fy + 60 * s, {240, 220, 170, 255});
}
}  // namespace ql
