#pragma once
#include "game.h"

namespace ql {
// UE game thread, called from the viewport's PostRender with its debug UCanvas.
void DrawPanel(game::UCanvas* canvas);
void DrawGallery(game::UCanvas* canvas);  // dev only (gallery.cpp)
void SetGamepadActive(bool gamepad);      // switches prompt glyphs between keycaps and Xbox buttons
}
