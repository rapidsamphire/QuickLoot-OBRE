#pragma once
// Minimal views of TES (legacy Oblivion) and UE5 types, and the engine functions the mod calls.
// Layouts come from the v0.411.193.0 PDB; every one used here was checked against the current exe
// (see MODLOG.md). Prefer functions/vfuncs over field offsets; layouts drift between builds.
#include <cstddef>
#include <cstdint>

namespace ql::game {

// ---------------- TES ----------------
struct TESForm;
struct TESBoundObject;
struct ExtraDataList;
struct Actor;
struct TESObjectREFR;

template <class T>
struct BSSimpleList {
    T item;
    BSSimpleList* next;
};

struct ItemChange {
    BSSimpleList<ExtraDataList*>* extraLists;
    int32_t count;
    TESBoundObject* object;
};

struct REFR_LOCK {
    int8_t baseLevel;
    void* key;
    uint8_t flags;  // bit 0 = locked
};

enum FormType : uint8_t {
    kFormType_CONT = 23,
    kFormType_NPC = 35,
    kFormType_CREA = 36,
};

inline uint8_t FormTypeOf(const void* form) { return *(reinterpret_cast<const uint8_t*>(form) + 8); }

// Virtual slots (byte offsets into the vtable).
namespace vt {
constexpr size_t RemoveItem = 0x230;
constexpr size_t GetObjectReference = 0x310;
constexpr size_t IsActor = 0x350;
constexpr size_t IsDead = 0x360;
constexpr size_t StealAlarm = 0x4E0;  // Character / PlayerCharacter
}

// TESObjectREFR::m_Extra (ExtraDataList). Same offset in both builds: TESObjectREFR::GetOwner's
// "add rcx, 88h" is part of its signature.
constexpr size_t kREFR_Extra = 0x88;
constexpr uint32_t OBJECT_ACTION_USE_DEFAULT = 1;

template <class Fn>
inline Fn VFunc(const void* obj, size_t slot)
{
    auto vtbl = *reinterpret_cast<uintptr_t* const*>(obj);
    return reinterpret_cast<Fn>(vtbl[slot / sizeof(uintptr_t)]);
}

TESBoundObject* GetBaseObject(TESObjectREFR* ref);
bool IsActor(TESObjectREFR* ref);
bool IsDead(TESObjectREFR* ref);
bool ActivatesByDefault(TESObjectREFR* ref);  // ExtraDataList::GetAction(OBJECT_ACTION_USE_DEFAULT)
TESObjectREFR* RemoveItem(TESObjectREFR* from, TESBoundObject* obj, ExtraDataList* extra, int count, bool keepOwner,
                          TESObjectREFR* to);
void StealAlarm(Actor* thief, TESObjectREFR* container, int value, TESForm* owner);

// ---------------- UE ----------------
struct UObject;
struct UFont;
struct UCanvas;

struct FString {
    const wchar_t* data;
    int32_t num;  // includes the terminator
    int32_t max;
};

struct FName {
    uint32_t index;
    uint32_t number;
    bool operator==(const FName& o) const { return index == o.index && number == o.number; }
};

struct FKey {
    FName name;
    void* details[2];
};

enum EInputEvent : uint8_t { IE_Pressed = 0, IE_Released = 1, IE_Repeat = 2, IE_DoubleClick = 3, IE_Axis = 4 };

struct FInputKeyEventArgs {
    void* viewport;
    int32_t controllerId;
    int32_t inputDevice;
    FKey key;
    EInputEvent event;
    float amountDepressed;
    bool isTouch;
};
static_assert(offsetof(FInputKeyEventArgs, key) == 0x10);
static_assert(offsetof(FInputKeyEventArgs, event) == 0x28);

struct FVector2D { double x, y; };
struct FLinearColor { float r, g, b, a; };

struct alignas(16) FCanvasTileItemStorage { uint8_t bytes[0xC0]; };  // FCanvasTileItem is 0xB0
constexpr size_t kCanvasItem_BlendMode = 0x1C;
constexpr uint32_t SE_BLEND_Translucent = 2;

struct FFontRenderInfo {
    uint32_t flags;  // bit0 bClipText, bit1 bEnableShadow
    uint8_t glow[0x3C];
};
static_assert(sizeof(FFontRenderInfo) == 0x40);

// UCanvas fields (engine layout, verified via UCanvas::DrawItem's +0x2E0 Canvas access).
constexpr size_t kUCanvas_ClipX = 0x30;      // float, visible width in canvas units
constexpr size_t kUCanvas_ClipY = 0x34;
constexpr size_t kUCanvas_DrawColor = 0x38;  // FColor (B,G,R,A)
constexpr size_t kUCanvas_SizeX = 0x40;
constexpr size_t kUCanvas_SizeY = 0x44;

// FText: TSharedRef<ITextData, ThreadSafe> + flags
struct FText {
    void* data;
    void* controller;
    uint32_t flags;
};
static_assert(sizeof(FText) == 0x18);

// ---------------- resolved functions ----------------
struct Functions {
    // TES
    void (*Main_OnIdle)(void* main);
    TESObjectREFR* (*Interface_GetActivateREFR)();
    bool (*Interface_IsInMenuMode)();
    int (*REFR_GetInventoryCount)(TESObjectREFR*, bool barter);
    ItemChange* (*REFR_GetInventoryItem)(TESObjectREFR*, int index, bool barter);
    bool (*ItemChange_ShouldDisplayItem)(ItemChange*, Actor*, bool barter, int rand, bool onlyPlayCheck, bool pickpocket);
    void (*ItemChange_dtor)(ItemChange*);
    void (*OperatorDelete)(void*, size_t);
    const char* (*ItemChange_GetFullName)(ItemChange*);
    int (*ItemChange_GetItemValue)(ItemChange*);
    TESForm* (*ItemChange_GetItemOwnership)(ItemChange*);
    const char* (*REFR_GetFullName)(TESObjectREFR*);
    TESForm* (*REFR_GetOwner)(TESObjectREFR*);
    bool (*REFR_IsAnOwner)(TESObjectREFR*, Actor*, bool useFaction);
    bool (*REFR_IsNPC)(TESObjectREFR*);
    REFR_LOCK* (*REFR_GetLock)(TESObjectREFR*);
    int16_t (*ExtraDataList_GetCount)(ExtraDataList*);
    void (*Actor_PlayPickUpSound)(Actor*, TESBoundObject*, bool pickUp, bool use, bool legacy);
    float (*GetFormWeight)(TESForm*);
    Actor** pPlayer;

    // UE
    void (*UGameViewportClient_Draw)(void* self, void* viewport, void* canvas);
    bool (*UCommonGameViewportClient_InputKey)(void* self, const FInputKeyEventArgs* args);
    float (*UCanvas_DrawText)(UCanvas*, const UFont*, const FString*, float x, float y, float sx, float sy,
                              const FFontRenderInfo*);
    void (*UCanvas_TextSize)(UCanvas*, const UFont*, const FString*, float* xl, float* yl, float sx, float sy);
    void (*UCanvas_DrawItem)(UCanvas*, void* item);
    void (*FCanvasTileItem_ctor)(void* self, const FVector2D* pos, const FVector2D* size, const FLinearColor* color);
    UFont* (*UEngine_GetMediumFont)();
    UFont* (*UEngine_GetLargeFont)();
    UFont* (*UEngine_GetSmallFont)();
    void (*ActivateInput_Pressed)(void* controller);
    void (*ActivateInput_Released)(void* controller);
    FName* (*FName_ctor)(FName* self, const wchar_t* name, int findType);

    // UE asset loading / textured drawing
    UObject* (*StaticLoadObject)(void* cls, UObject* outer, const wchar_t* name, const wchar_t* filename,
                                 uint32_t loadFlags, void* sandbox, bool allowReconciliation, void* instancingCtx);
    bool (*DoesPackageExist)(const FString* packageName, FString* outFilename, bool allowTextFormats);
    void* (*UTexture_GetResource)(UObject* texture);  // FTextureResource* (an FTexture)
    int32_t (*UTexture2D_GetSizeX)(UObject* texture);
    int32_t (*UTexture2D_GetSizeY)(UObject* texture);
    void (*FGCObject_Register)(void* gcObject);
    void (*FCanvasTileItem_ctorTex)(void* self, const FVector2D* pos, const void* texture, const FVector2D* size,
                                    const FLinearColor* color);
    void* (*UTexture2D_StaticClass)();
    void* (*UFont_StaticClass)();
    const char* (*ItemChange_GetIconOne)(ItemChange*, TESObjectREFR* owner);
    bool (*ExtraDataList_GetAction)(ExtraDataList* extra, uint32_t action);
    bool (*REFR_Activate)(TESObjectREFR* self, Actor* activator, bool idFlag, TESBoundObject* objectToGet, int count);

    // Localization + Slate-quality text
    FText* (*LocalizationManager_GetFTextFromKey)(FText* result, const char* key);
    const FString* (*FText_ToString)(const FText*);
    FText* (*FText_AsCultureInvariant)(FText* result, const FString* str);
    void (*FSlateFontInfo_ctor)(void* self, const UObject* font, float size, const FName* typeface, const void* outline);
    void (*FCanvasTextItem_ctor)(void* self, const FVector2D* pos, const FText* text, const UFont* font,
                                 const FLinearColor* color);
    void (*FCanvasTextItem_dtor)(void* self);
    void (*FName_ToString)(const FName* self, FString* out);  // allocates out->data with FMemory

    // GUObjectArray.ObjObjects (for AddToRoot)
    uint8_t*** GUObjectArray_Objects;  // FUObjectItem** chunks (65536 items each, 0x18 bytes)
    int32_t* GUObjectArray_NumElements;
};


// FCanvasTextItem (0x218) field offsets
struct alignas(16) FCanvasTextItemStorage { uint8_t bytes[0x230]; };
constexpr size_t kTextItem_FontRenderInfo = 0x48;
constexpr size_t kTextItem_ShadowColor = 0x88;   // FLinearColor
constexpr size_t kTextItem_ShadowOffset = 0x98;  // FVector2D
constexpr size_t kTextItem_DrawnSize = 0xA8;     // FVector2D, filled by Draw
constexpr size_t kTextItem_SlateFontInfo = 0x1B8;          // TOptional<FSlateFontInfo>
constexpr size_t kTextItem_SlateFontInfoIsSet = 0x1B8 + 0x58;

void AddToRoot(UObject* obj);  // sets EInternalObjectFlags::RootSet (bit 30) like UObject::AddToRoot

// FCanvasTileItem fields beyond FCanvasItem
constexpr size_t kTileItem_UV0 = 0x58;  // FVector2D
constexpr size_t kTileItem_UV1 = 0x68;  // FVector2D

extern Functions g_fn;
inline Actor* Player() { return g_fn.pPlayer ? *g_fn.pPlayer : nullptr; }

bool ResolveAll();  // fills g_fn from signatures; logs and returns false if anything is missing
FName MakeName(const wchar_t* s);

}  // namespace ql::game
