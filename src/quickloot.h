#pragma once
// Shared state between the TES thread (which owns inventories) and the UE game thread (draw + input).
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace ql {

struct LootItem {
    std::wstring name;
    std::wstring icon;        // legacy icon path (e.g. "Armor\Iron\M\Cuirass.dds")
    int count = 0;
    int value = 0;
    float weight = 0.0f;
    bool stolen = false;      // taking it is a crime
    int invIndex = 0;         // index for TESObjectREFR::GetInventoryItem
    uintptr_t object = 0;     // TESBoundObject*, used to re-validate the index before taking
};

struct LootSnapshot {
    uintptr_t ref = 0;        // TESObjectREFR* currently under the crosshair (0 = panel hidden)
    std::wstring title;
    bool isCorpse = false;
    bool stealing = false;
    std::vector<LootItem> items;
    uint64_t version = 0;     // bumps whenever the list is rebuilt
    uint64_t heartbeatMs = 0; // last TES tick; the panel hides if TES stops ticking (pause, loading)
};

enum class CommandType { TakeSelected, TakeAll, Open };

struct Command {
    CommandType type;
    uintptr_t ref;
    int invIndex;
    uintptr_t object;
};

struct Shared {
    std::mutex mutex;
    LootSnapshot snapshot;  // written by TES thread
    int selected = 0;       // owned by UE thread
    uintptr_t selectedRef = 0;
    std::vector<Command> commands;  // UE -> TES
};
extern Shared g_shared;

uint64_t NowMs();

// TES thread, once per Main::OnIdle.
void TesTick();

// UE thread helpers. PanelActive = there is something on screen that Activate would act on.
bool PanelActive(bool requireItems);
void QueueTake(bool all);
void QueueOpen();  // opens the full container menu (TESObjectREFR::Activate by the player)
void MoveSelection(int delta);

}  // namespace ql
