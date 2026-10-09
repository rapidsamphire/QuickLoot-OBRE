#include "quickloot.h"

#include <windows.h>
#include <algorithm>

#include "config.h"
#include "game.h"
#include "log.h"

namespace ql {
using namespace game;

Shared g_shared;

uint64_t NowMs() { return GetTickCount64(); }

namespace {
constexpr uint64_t kRebuildIntervalMs = 250;
constexpr uint64_t kStaleMs = 300;

std::wstring Widen(const char* s)
{
    if (!s || !*s) return {};
    // Names are UTF-8 in the remaster's data, but fall back to Windows-1252 for legacy plugin strings.
    for (UINT cp : {static_cast<UINT>(CP_UTF8), 1252u}) {
        DWORD flags = cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
        int n = MultiByteToWideChar(cp, flags, s, -1, nullptr, 0);
        if (n > 0) {
            std::wstring w(static_cast<size_t>(n - 1), L'\0');
            MultiByteToWideChar(cp, flags, s, -1, w.data(), n);
            return w;
        }
    }
    return {};
}

void FreeItem(ItemChange* ic)
{
    // GetInventoryItem returns a heap copy; ContainerMenu frees it exactly like this.
    g_fn.ItemChange_dtor(ic);
    g_fn.OperatorDelete(ic, sizeof(ItemChange));
}

TESObjectREFR* AsRef(Actor* a) { return reinterpret_cast<TESObjectREFR*>(a); }

bool IsLootable(TESObjectREFR* ref, bool& isCorpse)
{
    isCorpse = false;
    if (!ref || ref == AsRef(Player())) return false;
    TESBoundObject* base = GetBaseObject(ref);
    if (!base) return false;
    // A reference whose script overrides activation (an OnActivate block, e.g. the Emperor's body, where
    // Baurus stops you) carries ExtraAction without OBJECT_ACTION_USE_DEFAULT. TESObjectREFR::Activate then
    // runs the script instead of opening the inventory, so Quick Loot must not offer it either.
    if (!ActivatesByDefault(ref)) {
        static TESObjectREFR* s_lastSkipped = nullptr;
        if (g_config.debugLog && ref != s_lastSkipped) QL_LOG("skip %p: activation is scripted", ref);
        s_lastSkipped = ref;
        return false;
    }
    if (IsActor(ref)) {
        isCorpse = IsDead(ref);
        return isCorpse;  // living NPCs are pickpocketing, which keeps its own menu
    }
    if (FormTypeOf(base) != kFormType_CONT) return false;
    REFR_LOCK* lock = g_fn.REFR_GetLock(ref);
    return !(lock && (lock->flags & 1));
}

// Mirrors ContainerMenu::Create: taking from an owned container is theft, corpses never are.
bool IsStealing(TESObjectREFR* ref, bool isCorpse)
{
    if (isCorpse) return false;
    TESForm* owner = g_fn.REFR_GetOwner(ref);
    return owner && !g_fn.REFR_IsAnOwner(ref, Player(), true);
}

Actor* ActorOrNull(TESObjectREFR* ref) { return IsActor(ref) ? reinterpret_cast<Actor*>(ref) : nullptr; }

bool Displayable(TESObjectREFR* ref, ItemChange* ic)
{
    // Same filter ContainerMenu::UpdateList applies to a non-barter, non-pickpocket container.
    return ic->count > 0 && ic->object && g_fn.ItemChange_ShouldDisplayItem(ic, ActorOrNull(ref), false, 1, true, false);
}

bool ItemIsStolen(ItemChange* ic, bool stealing)
{
    if (!stealing) return false;
    auto playerBase = reinterpret_cast<TESForm*>(GetBaseObject(AsRef(Player())));
    return g_fn.ItemChange_GetItemOwnership(ic) != playerBase;
}

void BuildSnapshot(TESObjectREFR* ref, bool isCorpse, LootSnapshot& out)
{
    out.ref = reinterpret_cast<uintptr_t>(ref);
    out.isCorpse = isCorpse;
    out.stealing = IsStealing(ref, isCorpse);
    out.title = Widen(g_fn.REFR_GetFullName(ref));
    out.items.clear();

    int n = g_fn.REFR_GetInventoryCount(ref, false);
    for (int i = 0; i < n; ++i) {
        ItemChange* ic = g_fn.REFR_GetInventoryItem(ref, i, false);
        if (!ic) continue;
        if (Displayable(ref, ic)) {
            LootItem it;
            it.name = Widen(g_fn.ItemChange_GetFullName(ic));
            it.icon = Widen(g_fn.ItemChange_GetIconOne(ic, AsRef(Player())));  // as ContainerMenu::UpdateList
            it.count = ic->count;
            it.value = g_fn.ItemChange_GetItemValue(ic);
            it.weight = g_fn.GetFormWeight(reinterpret_cast<TESForm*>(ic->object));
            it.stolen = ItemIsStolen(ic, out.stealing);
            it.invIndex = i;
            it.object = reinterpret_cast<uintptr_t>(ic->object);
            if (it.name.empty()) it.name = L"<unnamed>";
            out.items.push_back(std::move(it));
        }
        FreeItem(ic);
    }
}

// Moves the whole stack at `ic` to the player, extra-data list by extra-data list, as ContainerMenu::DoClick does.
void Transfer(TESObjectREFR* ref, ItemChange* ic, bool keepOwner)
{
    Actor* player = Player();
    int remaining = ic->count;
    for (auto node = ic->extraLists; node && node->item && remaining > 0; node = node->next) {
        int c = g_fn.ExtraDataList_GetCount(node->item);
        if (c <= 0) continue;
        int take = std::min(c, remaining);
        RemoveItem(ref, ic->object, node->item, take, keepOwner, AsRef(player));
        remaining -= take;
    }
    if (remaining > 0) RemoveItem(ref, ic->object, nullptr, remaining, keepOwner, AsRef(player));
}

void RaiseCrime(TESObjectREFR* ref, int stolenValue)
{
    if (stolenValue > 0) StealAlarm(Player(), ref, stolenValue, g_fn.REFR_GetOwner(ref));
}

void TakeOne(TESObjectREFR* ref, const Command& cmd, bool stealing)
{
    ItemChange* ic = g_fn.REFR_GetInventoryItem(ref, cmd.invIndex, false);
    if (!ic) return;
    if (reinterpret_cast<uintptr_t>(ic->object) != cmd.object || !Displayable(ref, ic)) {
        QL_DEBUG("take: index %d no longer matches, ignored", cmd.invIndex);
        FreeItem(ic);
        return;
    }
    TESBoundObject* obj = ic->object;
    int stolenValue = ItemIsStolen(ic, stealing) ? g_fn.ItemChange_GetItemValue(ic) * ic->count : 0;
    if (g_config.debugLog) QL_LOG("take: idx=%d obj=%p count=%d stolenValue=%d", cmd.invIndex, obj, ic->count, stolenValue);
    Transfer(ref, ic, stealing);
    FreeItem(ic);
    g_fn.Actor_PlayPickUpSound(Player(), obj, true, false, true);
    RaiseCrime(ref, stolenValue);
}

void TakeAll(TESObjectREFR* ref, bool stealing)
{
    TESBoundObject* last = nullptr;
    int stolenValue = 0, taken = 0;
    int i = 0;
    for (int guard = 0; guard < 1024; ++guard) {
        int n = g_fn.REFR_GetInventoryCount(ref, false);
        if (i >= n) break;
        ItemChange* ic = g_fn.REFR_GetInventoryItem(ref, i, false);
        if (!ic) { ++i; continue; }
        if (!Displayable(ref, ic)) { FreeItem(ic); ++i; continue; }
        if (ItemIsStolen(ic, stealing)) stolenValue += g_fn.ItemChange_GetItemValue(ic) * ic->count;
        last = ic->object;
        Transfer(ref, ic, stealing);
        FreeItem(ic);
        ++taken;
        // The list shrinks when an entry is fully taken; if it didn't (quest item, refused move), step past it.
        if (g_fn.REFR_GetInventoryCount(ref, false) >= n) ++i;
    }
    if (last) g_fn.Actor_PlayPickUpSound(Player(), last, true, false, true);
    if (g_config.debugLog) QL_LOG("take all: %d entries, stolenValue=%d", taken, stolenValue);
    RaiseCrime(ref, stolenValue);
}
}  // namespace

void TesTick()
{
    const uint64_t now = NowMs();
    static uintptr_t s_lastRef = 0;
    static uint64_t s_lastBuild = 0;

    std::vector<Command> cmds;
    {
        std::lock_guard lock(g_shared.mutex);
        cmds.swap(g_shared.commands);
    }

    TESObjectREFR* ref = nullptr;
    bool isCorpse = false;
    if (g_config.enabled && Player() && !g_fn.Interface_IsInMenuMode()) {
        ref = g_fn.Interface_GetActivateREFR();
        if (!IsLootable(ref, isCorpse)) ref = nullptr;
    }

    if (!ref) {
        std::lock_guard lock(g_shared.mutex);
        g_shared.snapshot.ref = 0;
        g_shared.snapshot.items.clear();
        g_shared.snapshot.heartbeatMs = now;
        s_lastRef = 0;
        return;
    }

    const bool stealing = IsStealing(ref, isCorpse);
    for (const Command& c : cmds) {
        if (c.ref != reinterpret_cast<uintptr_t>(ref)) continue;  // crosshair moved since the key press
        if (c.type == CommandType::TakeAll)
            TakeAll(ref, stealing);
        else if (c.type == CommandType::Open) {
            bool ok = g_fn.REFR_Activate(ref, Player(), false, nullptr, 1);  // same args the game's AI activation uses
            QL_DEBUG("open: Activate(%p) -> %d", ref, ok);
        }
        else
            TakeOne(ref, c, stealing);
    }

    const uintptr_t refId = reinterpret_cast<uintptr_t>(ref);
    if (cmds.empty() && refId == s_lastRef && now - s_lastBuild < kRebuildIntervalMs) {
        std::lock_guard lock(g_shared.mutex);
        g_shared.snapshot.heartbeatMs = now;
        return;
    }

    LootSnapshot fresh;
    BuildSnapshot(ref, isCorpse, fresh);
    if (g_config.debugLog && refId != s_lastRef)
        QL_LOG("target %p '%ls' corpse=%d stealing=%d items=%zu", ref, fresh.title.c_str(), isCorpse, fresh.stealing,
               fresh.items.size());
    s_lastRef = refId;
    s_lastBuild = now;

    std::lock_guard lock(g_shared.mutex);
    fresh.version = g_shared.snapshot.version + 1;
    fresh.heartbeatMs = now;
    g_shared.snapshot = std::move(fresh);
}

bool PanelActive(bool requireItems)
{
    std::lock_guard lock(g_shared.mutex);
    const LootSnapshot& s = g_shared.snapshot;
    if (!s.ref || NowMs() - s.heartbeatMs > kStaleMs) return false;
    if (requireItems) return !s.items.empty();
    return !s.items.empty() || g_config.showEmpty;
}

void MoveSelection(int delta)
{
    std::lock_guard lock(g_shared.mutex);
    int n = static_cast<int>(g_shared.snapshot.items.size());
    if (n == 0) return;
    g_shared.selected = std::clamp(g_shared.selected + delta, 0, n - 1);
}

void QueueTake(bool all)
{
    std::lock_guard lock(g_shared.mutex);
    const LootSnapshot& s = g_shared.snapshot;
    if (!s.ref || s.items.empty()) return;
    if (all) {
        g_shared.commands.push_back({CommandType::TakeAll, s.ref, 0, 0});
        return;
    }
    int sel = std::clamp(g_shared.selected, 0, static_cast<int>(s.items.size()) - 1);
    const LootItem& it = s.items[static_cast<size_t>(sel)];
    g_shared.commands.push_back({CommandType::TakeSelected, s.ref, it.invIndex, it.object});
}
void QueueOpen()
{
    std::lock_guard lock(g_shared.mutex);
    if (g_shared.snapshot.ref) g_shared.commands.push_back({CommandType::Open, g_shared.snapshot.ref, 0, 0});
}
}  // namespace ql
