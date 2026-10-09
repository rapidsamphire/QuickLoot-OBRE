// Oblivion Remastered Quick Loot: FO4-style loot panel for containers and corpses.
#include <windows.h>
#include <MinHook.h>

#include <eh.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "game.h"
#include "log.h"
#include "quickloot.h"
#include "render.h"
#include "scan.h"

#define QL_VERSION "1.0.0"

using namespace ql;
using namespace ql::game;

namespace {
HMODULE g_self;
std::wstring g_iniPath;
std::atomic<bool> g_faulted{false};

decltype(Functions::Main_OnIdle) o_OnIdle;
decltype(Functions::UGameViewportClient_Draw) o_Draw;
decltype(Functions::UCommonGameViewportClient_InputKey) o_InputKey;
decltype(Functions::ActivateInput_Pressed) o_ActivatePressed;
decltype(Functions::ActivateInput_Released) o_ActivateReleased;
void (*o_PostRender)(void* self, UCanvas* canvas);

// The game's code is not ours to crash: any fault inside the mod disables it (built with /EHa).
struct SehError {
    unsigned code;
    uintptr_t address;
    uintptr_t accessAddress;
};

void SehTranslator(unsigned code, EXCEPTION_POINTERS* ep)
{
    auto rec = ep->ExceptionRecord;
    throw SehError{code, reinterpret_cast<uintptr_t>(rec->ExceptionAddress),
                   rec->NumberParameters >= 2 ? rec->ExceptionInformation[1] : 0};
}

std::string DescribeAddress(uintptr_t a)
{
    HMODULE mod = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(a), &mod))
        GetModuleFileNameA(mod, name, MAX_PATH);
    const char* base = strrchr(name, '\\');
    char buf[MAX_PATH + 32];
    snprintf(buf, sizeof(buf), "%s+%llx", base ? base + 1 : name,
             static_cast<unsigned long long>(a - reinterpret_cast<uintptr_t>(mod)));
    return buf;
}

template <class F>
void Guarded(const char* where, F&& f)
{
    if (g_faulted) return;
    auto previous = _set_se_translator(SehTranslator);
    try {
        f();
    } catch (const SehError& e) {
        g_faulted = true;
        g_config.enabled = false;
        QL_LOG("FAULT in %s: exception %08X at %s (access %llx) - Quick Loot disabled for this session", where, e.code,
               DescribeAddress(e.address).c_str(), static_cast<unsigned long long>(e.accessAddress));
    } catch (...) {
        g_faulted = true;
        g_config.enabled = false;
        QL_LOG("FAULT in %s - Quick Loot disabled for this session", where);
    }
    _set_se_translator(previous);
}

// ---------------- TES thread ----------------
std::atomic<uint32_t> g_idleCount{0};

void Hook_OnIdle(void* main)
{
    o_OnIdle(main);
    if (g_idleCount++ == 0) QL_DEBUG("Main::OnIdle hook live (TES thread)");
    Guarded("TesTick", [] { TesTick(); });
}

// ---------------- UE game thread: activate ----------------
// Activate takes the selected item. It's intercepted as a raw key in InputKey ([Keys] Activate); this hook
// is the fallback for an activate binding not listed there.
bool g_swallowActivateRelease = false;

void Hook_ActivatePressed(void* controller)
{
    bool intercept = false;
    Guarded("ActivatePressed", [&] { intercept = g_config.enabled && PanelActive(true); });
    if (!intercept) {
        o_ActivatePressed(controller);
        return;
    }
    g_swallowActivateRelease = true;
    Guarded("QueueTake", [] { QueueTake(false); });
}

void Hook_ActivateReleased(void* controller)
{
    if (g_swallowActivateRelease) {
        g_swallowActivateRelease = false;
        return;
    }
    o_ActivateReleased(controller);
}

// ---------------- UE game thread: keys ----------------
struct KeyNames {
    std::vector<FName> up, down, takeAll, activate, open;
    std::vector<FName> swallowed;  // keys whose press we consumed; also consume their release
    bool ready = false;
} g_keys;

bool Contains(const std::vector<FName>& v, const FName& n)
{
    for (auto& x : v)
        if (x == n) return true;
    return false;
}

// Gamepad FKeys all start with "Gamepad_"; FName comparison indices aren't ordered, so keep a set of the
// common ones and check membership.
std::vector<FName> g_padKeys;

bool IsGamepadKey(const FName& key) { return Contains(g_padKeys, key); }

void BuildKeyNames()
{
    for (const wchar_t* n :
         {L"Gamepad_FaceButton_Bottom", L"Gamepad_FaceButton_Right", L"Gamepad_FaceButton_Left",
          L"Gamepad_FaceButton_Top", L"Gamepad_DPad_Up", L"Gamepad_DPad_Down", L"Gamepad_DPad_Left",
          L"Gamepad_DPad_Right", L"Gamepad_LeftShoulder", L"Gamepad_RightShoulder", L"Gamepad_LeftTrigger",
          L"Gamepad_RightTrigger", L"Gamepad_Special_Right", L"Gamepad_Special_Left", L"Gamepad_LeftThumbstick",
          L"Gamepad_RightThumbstick"})
        g_padKeys.push_back(MakeName(n));
    auto make = [](const std::vector<std::wstring>& names) {
        std::vector<FName> out;
        for (auto& n : names) out.push_back(MakeName(n.c_str()));
        return out;
    };
    g_keys.up = make(g_config.keysUp);
    g_keys.down = make(g_config.keysDown);
    g_keys.takeAll = make(g_config.keysTakeAll);
    g_keys.activate = make(g_config.keysActivate);
    g_keys.open = make(g_config.keysOpen);
    g_keys.ready = true;
}

bool Hook_InputKey(void* self, const FInputKeyEventArgs* args)
{
    bool consumed = false;
    Guarded("InputKey", [&] {
        if (!g_keys.ready) BuildKeyNames();
        const FName key = args->key.name;
        if (args->event == IE_Pressed) SetGamepadActive(IsGamepadKey(key));
        if (args->event == IE_Released) {
            for (auto it = g_keys.swallowed.begin(); it != g_keys.swallowed.end(); ++it)
                if (*it == key) { g_keys.swallowed.erase(it); consumed = true; break; }
            return;
        }
        if ((args->event != IE_Pressed && args->event != IE_Repeat) || !g_config.enabled || !PanelActive(true)) return;
        if (Contains(g_keys.up, key)) { MoveSelection(-1); consumed = true; }
        else if (Contains(g_keys.down, key)) { MoveSelection(+1); consumed = true; }
        else if (Contains(g_keys.activate, key)) {
            if (args->event == IE_Pressed) QueueTake(false);
            consumed = true;
        } else if (Contains(g_keys.takeAll, key)) {
            if (args->event == IE_Pressed) QueueTake(true);
            consumed = true;
        } else if (Contains(g_keys.open, key)) {
            if (args->event == IE_Pressed) QueueOpen();  // full container menu (TESObjectREFR::Activate)
            consumed = true;
        }
        if (consumed && !Contains(g_keys.swallowed, key)) g_keys.swallowed.push_back(key);
    });
    return consumed ? true : o_InputKey(self, args);
}

// ---------------- UE game thread: drawing ----------------
void Hook_PostRender(void* self, UCanvas* canvas)
{
    o_PostRender(self, canvas);
    Guarded("PostRender", [&] {
        if (g_config.gallery) {
            static uint64_t lastReload = 0;
            if (NowMs() - lastReload > 1000) {  // dev: lets the gallery page be flipped by editing the ini
                lastReload = NowMs();
                LoadConfig(g_iniPath);
            }
            DrawGallery(canvas);
        }
        DrawPanel(canvas);
    });
}

// UGameViewportClient::PostRender is a 10-byte ICF-folded stub shared by hundreds of vtables, so patch only
// this viewport client's vtable slot instead of hooking the code.
constexpr size_t kPostRenderSlot = 0x380;
constexpr const char* kPostRenderStub = "48 8B 01 48 FF A0 88 03 00 00";

bool PatchPostRender(void* drawThis)
{
    for (intptr_t adjust : {-0x28, 0}) {  // Draw receives the FViewportClient subobject at +0x28
        auto obj = reinterpret_cast<uintptr_t>(drawThis) + adjust;
        auto vtbl = *reinterpret_cast<uintptr_t**>(obj);
        uintptr_t* slot = vtbl + kPostRenderSlot / sizeof(uintptr_t);
        if (!scan::Matches(*slot, kPostRenderStub)) continue;
        DWORD old;
        VirtualProtect(slot, sizeof(uintptr_t), PAGE_READWRITE, &old);
        o_PostRender = reinterpret_cast<decltype(o_PostRender)>(*slot);
        *slot = reinterpret_cast<uintptr_t>(&Hook_PostRender);
        VirtualProtect(slot, sizeof(uintptr_t), old, &old);
        QL_DEBUG("PostRender vtable slot patched (viewport client %p, adjust %lld)", reinterpret_cast<void*>(obj),
               static_cast<long long>(adjust));
        return true;
    }
    QL_LOG("ERROR: could not locate PostRender slot; panel will not draw");
    return false;
}

void Hook_Draw(void* self, void* viewport, void* canvas)
{
    static bool tried = false;
    if (!tried) {
        tried = true;
        Guarded("PatchPostRender", [&] { PatchPostRender(self); });
    }
    o_Draw(self, viewport, canvas);
}

// ---------------- init ----------------
std::wstring ModuleDir()
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH);
    std::wstring s(path, n);
    return s.substr(0, s.find_last_of(L'\\') + 1);
}

template <class T>
bool Hook(const char* name, T target, T detour, T* original)
{
    MH_STATUS st = MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(detour),
                                 reinterpret_cast<void**>(original));
    if (st != MH_OK) {
        QL_LOG("hook %s failed: %s", name, MH_StatusToString(st));
        return false;
    }
    return true;
}

DWORD WINAPI InitThread(LPVOID)
{
    const std::wstring dir = ModuleDir();
    log::Init(dir + L"OblivionQuickLoot.log");
    g_iniPath = dir + L"OblivionQuickLoot.ini";
    LoadConfig(g_iniPath);
    QL_LOG("Oblivion Quick Loot " QL_VERSION " loading");

    if (!ResolveAll()) {
        QL_LOG("ERROR: unsupported game version (signatures missing). Nothing was hooked.");
        return 0;
    }
    if (MH_Initialize() != MH_OK) {
        QL_LOG("ERROR: MinHook init failed");
        return 0;
    }
    bool ok = Hook("Main::OnIdle", g_fn.Main_OnIdle, &Hook_OnIdle, &o_OnIdle) &&
              Hook("UGameViewportClient::Draw", g_fn.UGameViewportClient_Draw, &Hook_Draw, &o_Draw) &&
              Hook("UCommonGameViewportClient::InputKey", g_fn.UCommonGameViewportClient_InputKey, &Hook_InputKey,
                   &o_InputKey) &&
              Hook("ActivateInput_Pressed", g_fn.ActivateInput_Pressed, &Hook_ActivatePressed, &o_ActivatePressed) &&
              Hook("ActivateInput_Released", g_fn.ActivateInput_Released, &Hook_ActivateReleased, &o_ActivateReleased);
    if (!ok || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        QL_LOG("ERROR: hooks not installed");
        MH_DisableHook(MH_ALL_HOOKS);
        return 0;
    }
    QL_LOG("hooks installed");
    return 0;
}
}  // namespace

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = self;
        DisableThreadLibraryCalls(self);
        if (HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
