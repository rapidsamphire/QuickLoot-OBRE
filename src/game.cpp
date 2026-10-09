#include "game.h"

#include <windows.h>

#include "config.h"
#include "log.h"
#include "scan.h"

namespace ql::game {
Functions g_fn;

namespace {
// Signatures generated from the current Steam exe (buildid 19115871) and checked to be unique there.
// Each maps to a PDB-named function in the v0.411.193.0 reference build (see MODLOG.md).
struct Sig {
    const char* name;
    const char* pattern;
    void** out;
};

#define SIG(field, pat) {#field, pat, reinterpret_cast<void**>(&g_fn.field)}
const Sig kSigs[] = {
    SIG(Main_OnIdle, "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 60 48 8B E9 48"),
    SIG(Interface_GetActivateREFR, "48 83 EC 28 B2 01 33 C9 E8 ?? ?? ?? ?? 48 85 C0 74 ?? B2 01 33 C9 E8 ?? ?? ?? ?? 48 83 78 30 00 74 ?? B2 01 33 C9 E8 ?? ?? ?? ?? 48 83 B8 A0 00 00 00 00 74 ?? B2 01 33 C9 E8 ?? ?? ?? ?? BA AE 0F 00 00 48 8B 88 A0 00 00 00 E8 ?? ?? ?? ?? 0F 2E 05 ?? ?? ?? ?? 7A ?? 75 ?? B2 01 33 C9 E8 ?? ?? ?? ?? 48 8B 80 58 01"),
    SIG(Interface_IsInMenuMode, "48 83 EC 28 B2 01 33 C9 E8 ?? ?? ?? ?? 48 85 C0 74 ?? B2 01 33 C9 E8 ?? ?? ?? ?? 48 83 78 30 00 74 ?? B2 01 33 C9 E8 ?? ?? ?? ?? 80 78 10 01 0F"),
    SIG(REFR_GetInventoryCount, "48 89 5C 24 08 57 48 83 EC 20 0F B6 FA 48 8B D9 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 40 84"),
    SIG(REFR_GetInventoryItem, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 0F B6 F8 8B F2 48 8B D9 E8 ?? ?? ?? ?? 48 85 C0"),
    SIG(ItemChange_ShouldDisplayItem, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 48 8B F1 41 0F B6 E8 48"),
    SIG(ItemChange_dtor, "48 89 74 24 10 57 48 83 EC 20 48 8B 39 48 8B F1 48 85 FF 74 ?? 48 8B 4F"),
    SIG(ItemChange_GetFullName, "48 83 EC 38 48 8B 49 10 4C 8D 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? C7 44 24 20 00 00 00 00 33 D2 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 0F B7 48 10 BA FF"),
    SIG(ItemChange_GetItemValue, "48 8B 49 10 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC 89 54 24 10"),
    SIG(ItemChange_GetItemOwnership, "40 53 48 83 EC 20 48 8B 19 48 85 DB 74 ?? 48 8B 1B 48 85 DB 74 ?? 48 8B CB E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B CB 48 83 C4 20 5B E9 ?? ?? ?? ?? 33 C0 48 83 C4 20 5B C3 CC CC CC CC CC CC CC CC 40 53 48 83"),
    SIG(REFR_GetFullName, "48 83 EC 38 48 8B 49 50 4C 8D 0D ?? ?? ?? ?? 4C"),
    SIG(REFR_GetOwner, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B D9 48 81 C1 88 00 00"),
    SIG(REFR_IsAnOwner, "40 53 41 55 41 56 48 83 EC 40 45 0F B6 E8 4C 8B"),
    SIG(REFR_IsNPC, "40 53 48 83 EC 20 48 8B 01 48 8B D9 FF 90 50 03 00 00 84 C0 74 ?? 48 8B 03 48 8B CB FF 90 10 03"),
    SIG(ExtraDataList_GetCount, "48 83 EC 28 B2 2A E8 ?? ?? ?? ?? 48 85 C0 74 ??"),
    SIG(Actor_PlayPickUpSound, "48 85 D2 0F 84 ?? ?? ?? ?? 53 55 56 57 48 83 EC"),
    SIG(GetFormWeight, "48 85 C9 74 ?? 0F B6 41 08 83 C0 ED 83 F8 17 77"),
    SIG(UGameViewportClient_Draw, "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 50 F9 FF FF 48 81 EC B0 07 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 10 06 00 00"),
    SIG(UCommonGameViewportClient_InputKey, "48 89 5C 24 18 55 56 57 48 8D 6C 24 90 48 81 EC 70 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 60 48 8B"),
    SIG(UCanvas_DrawText, "48 89 5C 24 08 57 48 83 EC 70 48 8B DA 0F 29 74"),
    SIG(UCanvas_TextSize, "48 85 D2 0F 84 ?? ?? ?? ?? 53 48 83 EC 70 41 83"),
    SIG(UCanvas_DrawItem, "40 53 48 83 EC 20 48 8B 99 E0 02 00 00 4C 8B C2 48 8B 02 49 8B C8 48 8B D3 FF 50 18"),
    SIG(FCanvasTileItem_ctor, "48 8D 05 ?? ?? ?? ?? 48 89 01 48 8D 05 ?? ?? ?? ?? 0F 10 02 33 D2 48 89 51 18 0F 11 41 08 88 51 20 48 89 51 28 0F 10 05 ?? ?? ?? ?? 48 89 01 48 B8 00 00 00 00 00 00 F0 3F 0F 11 41 30 41 0F 10 00 C7 41 50"),
    SIG(UEngine_GetMediumFont, "48 8B 05 ?? ?? ?? ?? 48 8B 80 80 00 00 00 C3 CC"),
    SIG(UEngine_GetLargeFont, "48 8B 05 ?? ?? ?? ?? 48 8B 80 A8 00 00 00 C3 CC"),
    SIG(UEngine_GetSmallFont, "48 8B 05 ?? ?? ?? ?? 48 8B 40 58 C3 CC CC CC CC"),
    SIG(ActivateInput_Pressed, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B D9 48 81 C1 5C 09 00"),
    SIG(ActivateInput_Released, "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? BA 05"),
    SIG(FName_ctor, "48 89 5C 24 08 57 48 83 EC 30 48 8B D9 48 89 54 24 20 33 C9"),
    SIG(StaticLoadObject, "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? 48 81 EC 58 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 48 01 00 00 48 8B 85"),
    SIG(DoesPackageExist, "48 89 5C 24 08 48 89 74 24 18 55 57 41 56 48 8D AC 24 ?? ?? ?? ?? 48 81 EC 50 05 00"),
    SIG(UTexture_GetResource, "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 84 C0 75 ?? E8 ?? ?? ?? ?? 84 C0 75 ?? E8"),
    SIG(UTexture2D_GetSizeX, "48 8B 81 D8 01 00 00 48 85 C0 74 ?? 8B 00 C3 C3"),
    SIG(UTexture2D_GetSizeY, "48 8B 81 D8 01 00 00 48 85 C0 74 ?? 8B 40 04 C3"),
    SIG(FGCObject_Register, "4C 8B DC 55 48 81 EC C0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 B0 00 00"),
    SIG(FCanvasTileItem_ctorTex, "48 8D 05 ?? ?? ?? ?? 48 89 01 48 8D 05 ?? ?? ?? ?? 0F 10 02 33 D2 48 89 51 18 0F 11 41 08 88 51 20 48 89 51 28 0F 10 05 ?? ?? ?? ?? 48 89 01 48 B8 00 00 00 00 00 00 F0 3F 0F 11 41 30 41 0F 10 01 C7 41 50 00 00 80 3F 4C 89 41 78"),
    SIG(ExtraDataList_GetAction, "40 53 48 83 EC 20 8B DA B2 13 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 0F B6 40 18 85 C3"),
    SIG(REFR_Activate, "40 53 55 56 41 54 41 56 41 57 48 81 EC 58 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 40 01 00 00 32"),
    SIG(ItemChange_GetIconOne, "48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 30 48 8B 19 48 8B EA 48 8B F9 48"),
    SIG(LocalizationManager_GetFTextFromKey, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 48 8B D9 48 8D 4C 24 30 E8 ?? ?? ?? ?? 48 8B 7C 24"),
    SIG(FText_ToString, "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 48 8B 0B 48 8B 01 48 83 C4 20"),
    SIG(FSlateFontInfo_ctor, "48 89 5C 24 08 57 48 83 EC 20 48 8B 44 24 50 48 8B D9 F3 0F 5D 15 ?? ?? ?? ?? 48 89"),
    SIG(FCanvasTextItem_ctor, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 5C 24 50 48 8D 05"),
    SIG(FName_ToString, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 80 3D ?? ?? ?? ?? 00 48 8B FA 8B 19 48 8B F1 74 ?? 48 8D 15"),
    SIG(FCanvasTextItem_dtor,"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8D 05 ?? ?? ?? ?? 48 8B D9 48 89 01 BE FF FF FF FF 80 B9 10"),
};
#undef SIG

// Indirect resolutions.
constexpr const char* kSig_Actor_IsPc = "48 3B 0D ?? ?? ?? ?? 0F 94 C0 C3 CC CC CC CC CC";  // cmp rcx,[pPlayer]
constexpr const char* kSig_REFR_GetLockLevel =
    "48 83 EC 28 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B C8 48 83 C4 28 E9 ?? ?? ?? ?? 48 83 C4 28 C3 CC 48 8B C4 48";
// Class getters are generic-looking code; reach them through unique callers instead.
// UBrushBinding::IsSupportedSource +34: call UTexture2D::GetPrivateStaticClass
constexpr const char* kSig_Tex2DClassCaller =
    "40 53 48 83 EC 20 48 8B DA 48 85 D2 74 ?? 48 8B 42 08 8B 48 10 48 C1 E9 10 F6 C1 01";
constexpr int kOff_Tex2DClassCall = 34;
// UEngine::InitializeObjectReferences font-loading lambda +92: call UFont::StaticClass
constexpr const char* kSig_FontClassCaller =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 81 EC E0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 D0 00 00 00 41";
constexpr int kOff_FontClassCall = 92;

constexpr const char* kSig_FTextFromStringTwin =
    "48 89 5C 24 18 48 89 6C 24 20 57 48 83 EC 60 33 C0 48 89 74 24 78 48 63 72 08 48 8B F9 89 44 24 70 83 FE 01 7F ?? E8";

template <class T>
bool ResolveCall(const char* sig, int callOffset, T& out)
{
    uintptr_t a = scan::Find(sig);
    if (!a || *reinterpret_cast<uint8_t*>(a + callOffset) != 0xE8) return false;
    out = reinterpret_cast<T>(scan::Rel32(a + callOffset + 1));
    return true;
}
}  // namespace

bool ResolveAll()
{
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    bool ok = true;
    for (const Sig& s : kSigs) {
        uintptr_t a = scan::Find(s.pattern);
        *s.out = reinterpret_cast<void*>(a);
        if (a)
            QL_DEBUG("  %-36s exe+%llx", s.name, static_cast<unsigned long long>(a - base));
        else {
            QL_LOG("  %-36s NOT FOUND", s.name);
            ok = false;
        }
    }

    if (uintptr_t isPc = scan::Find(kSig_Actor_IsPc))
        g_fn.pPlayer = reinterpret_cast<Actor**>(scan::Rel32(isPc + 3));
    if (uintptr_t lockLevel = scan::Find(kSig_REFR_GetLockLevel))
        g_fn.REFR_GetLock = reinterpret_cast<decltype(g_fn.REFR_GetLock)>(scan::Rel32(lockLevel + 5));
    QL_DEBUG("  %-36s exe+%llx", "pPlayer", g_fn.pPlayer ? static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_fn.pPlayer) - base) : 0ull);
    QL_DEBUG("  %-36s exe+%llx", "REFR_GetLock", g_fn.REFR_GetLock ? static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_fn.REFR_GetLock) - base) : 0ull);
    // operator delete[](void*, size_t) is a 5-byte jmp stub whose surrounding padding differs between the
    // Steam and Game Pass builds. Find it the way the game uses it instead: every ItemChange free is
    //   call ItemChange::~ItemChange ; mov edx, 18h ; mov rcx, reg ; call operator delete
    // (160 such sites in the Steam build, all agreeing). Take the delete from sites whose first call is
    // the already-resolved destructor.
    if (g_fn.ItemChange_dtor) {
        for (uintptr_t site : scan::FindAll("E8 ?? ?? ?? ?? BA 18 00 00 00 48 8B ?? E8", 4096)) {
            if (scan::Rel32(site + 1) != reinterpret_cast<uintptr_t>(g_fn.ItemChange_dtor)) continue;
            g_fn.OperatorDelete = reinterpret_cast<decltype(g_fn.OperatorDelete)>(scan::Rel32(site + 14));
            break;
        }
    }
    if (g_fn.OperatorDelete) QL_DEBUG("  %-36s ok", "OperatorDelete (via ~ItemChange sites)"); else QL_LOG("  %-36s NOT FOUND", "OperatorDelete (via ~ItemChange sites)");
    ok = ok && g_fn.OperatorDelete;

    bool cls = ResolveCall(kSig_Tex2DClassCaller, kOff_Tex2DClassCall, g_fn.UTexture2D_StaticClass) &&
               ResolveCall(kSig_FontClassCaller, kOff_FontClassCall, g_fn.UFont_StaticClass);
    if (cls) QL_DEBUG("  %-36s ok", "UTexture2D/UFont class getters"); else QL_LOG("  %-36s NOT FOUND", "UTexture2D/UFont class getters");

    // FText::AsCultureInvariant(const FString&) and FText::FromString(const FString&) compile to identical
    // code in this build; either works for display text, take the first.
    g_fn.FText_AsCultureInvariant = reinterpret_cast<decltype(g_fn.FText_AsCultureInvariant)>(
        scan::FindNth(kSig_FTextFromStringTwin, 0, 2));
    if (g_fn.FText_AsCultureInvariant) QL_DEBUG("  %-36s ok", "FText::AsCultureInvariant"); else QL_LOG("  %-36s NOT FOUND", "FText::AsCultureInvariant");

    // GUObjectArray, read from FGCObject::RegisterGCObject's own AddToRoot sequence:
    //   +0xB9  3B 05 rel32   cmp eax, [GUObjectArray.ObjObjects.NumElements]
    //   +0xCE  48 8B 05 rel32 mov rax, [GUObjectArray.ObjObjects.Objects]
    auto reg = reinterpret_cast<uint8_t*>(g_fn.FGCObject_Register);
    bool gua = false;
    if (reg && reg[0xB9] == 0x3B && reg[0xBA] == 0x05 && reg[0xCE] == 0x48 && reg[0xCF] == 0x8B && reg[0xD0] == 0x05) {
        g_fn.GUObjectArray_NumElements = reinterpret_cast<int32_t*>(scan::Rel32(reinterpret_cast<uintptr_t>(reg + 0xBB)));
        g_fn.GUObjectArray_Objects = reinterpret_cast<uint8_t***>(scan::Rel32(reinterpret_cast<uintptr_t>(reg + 0xD1)));
        gua = true;
    }
    if (gua) QL_DEBUG("  %-36s ok", "GUObjectArray"); else QL_LOG("  %-36s NOT FOUND", "GUObjectArray");
    return ok && cls && gua && g_fn.FText_AsCultureInvariant && g_fn.pPlayer && g_fn.REFR_GetLock;
}

void AddToRoot(UObject* obj)
{
    if (!obj) return;
    const int32_t index = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(obj) + 0xC);  // InternalIndex
    if (index < 0 || index >= *g_fn.GUObjectArray_NumElements) return;
    uint8_t* chunk = (*g_fn.GUObjectArray_Objects)[index >> 16];
    auto flags = reinterpret_cast<volatile long*>(chunk + (index & 0xFFFF) * 0x18 + 8);  // FUObjectItem::Flags
    _InterlockedOr(flags, 1 << 30);  // EInternalObjectFlags::RootSet
}

FName MakeName(const wchar_t* s)
{
    FName n{};
    g_fn.FName_ctor(&n, s, 1 /*FNAME_Add*/);
    return n;
}

TESBoundObject* GetBaseObject(TESObjectREFR* ref)
{
    return VFunc<TESBoundObject* (*)(TESObjectREFR*)>(ref, vt::GetObjectReference)(ref);
}

bool IsActor(TESObjectREFR* ref)
{
    return VFunc<bool (*)(TESObjectREFR*)>(ref, vt::IsActor)(ref);
}

bool ActivatesByDefault(TESObjectREFR* ref)
{
    auto extra = reinterpret_cast<ExtraDataList*>(reinterpret_cast<uint8_t*>(ref) + kREFR_Extra);
    return g_fn.ExtraDataList_GetAction(extra, OBJECT_ACTION_USE_DEFAULT);
}

bool IsDead(TESObjectREFR* ref)
{
    return VFunc<bool (*)(TESObjectREFR*, bool)>(ref, vt::IsDead)(ref, false);
}

TESObjectREFR* RemoveItem(TESObjectREFR* from, TESBoundObject* obj, ExtraDataList* extra, int count, bool keepOwner,
                          TESObjectREFR* to)
{
    using Fn = TESObjectREFR* (*)(TESObjectREFR*, TESBoundObject*, ExtraDataList*, int, bool, bool, TESObjectREFR*,
                                  void*, void*, bool, bool);
    // Same argument shape ContainerMenu::DoClick uses for "take".
    return VFunc<Fn>(from, vt::RemoveItem)(from, obj, extra, count, keepOwner, false, to, nullptr, nullptr, true, false);
}

void StealAlarm(Actor* thief, TESObjectREFR* container, int value, TESForm* owner)
{
    using Fn = void (*)(Actor*, TESObjectREFR*, TESBoundObject*, int, int, TESForm*);
    VFunc<Fn>(thief, vt::StealAlarm)(thief, container, nullptr, 0, value, owner);
}
}  // namespace ql::game
