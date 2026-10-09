#include "scan.h"

#include <windows.h>
#include <cstring>
#include <vector>

namespace ql::scan {
namespace {
struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask;  // true = must match
};

Pattern Parse(const char* s)
{
    Pattern p;
    while (*s) {
        if (*s == ' ') { ++s; continue; }
        if (s[0] == '?') {
            p.bytes.push_back(0);
            p.mask.push_back(false);
            s += (s[1] == '?') ? 2 : 1;
            continue;
        }
        char hex[3] = {s[0], s[1], 0};
        p.bytes.push_back(static_cast<uint8_t>(strtoul(hex, nullptr, 16)));
        p.mask.push_back(true);
        s += 2;
    }
    return p;
}

bool TextSection(uintptr_t& begin, uintptr_t& end)
{
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 6) == 0) {
            begin = base + sec->VirtualAddress;
            end = begin + sec->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

bool MatchAt(const uint8_t* p, const Pattern& pat)
{
    for (size_t i = 0; i < pat.bytes.size(); ++i)
        if (pat.mask[i] && p[i] != pat.bytes[i]) return false;
    return true;
}
}  // namespace

uintptr_t Find(const char* pattern)
{
    uintptr_t begin, end;
    if (!TextSection(begin, end)) return 0;
    Pattern pat = Parse(pattern);
    if (pat.bytes.empty() || !pat.mask[0]) return 0;

    const uint8_t first = pat.bytes[0];
    const size_t n = pat.bytes.size();
    uintptr_t found = 0;
    for (auto p = reinterpret_cast<const uint8_t*>(begin); p + n <= reinterpret_cast<const uint8_t*>(end); ++p) {
        p = static_cast<const uint8_t*>(memchr(p, first, reinterpret_cast<const uint8_t*>(end) - p - n + 1));
        if (!p) break;
        if (MatchAt(p, pat)) {
            if (found) return 0;  // not unique: refuse rather than guess
            found = reinterpret_cast<uintptr_t>(p);
        }
    }
    return found;
}

std::vector<uintptr_t> FindAll(const char* pattern, size_t limit)
{
    std::vector<uintptr_t> hits;
    uintptr_t begin, end;
    if (!TextSection(begin, end)) return hits;
    Pattern pat = Parse(pattern);
    if (pat.bytes.empty() || !pat.mask[0]) return hits;
    const size_t len = pat.bytes.size();
    for (auto p = reinterpret_cast<const uint8_t*>(begin); p + len <= reinterpret_cast<const uint8_t*>(end); ++p) {
        p = static_cast<const uint8_t*>(memchr(p, pat.bytes[0], reinterpret_cast<const uint8_t*>(end) - p - len + 1));
        if (!p) break;
        if (MatchAt(p, pat)) {
            hits.push_back(reinterpret_cast<uintptr_t>(p));
            if (hits.size() >= limit) break;
        }
    }
    return hits;
}

uintptr_t FindNth(const char* pattern, int n, int expected)
{
    uintptr_t begin, end;
    if (!TextSection(begin, end)) return 0;
    Pattern pat = Parse(pattern);
    if (pat.bytes.empty() || !pat.mask[0]) return 0;
    const size_t len = pat.bytes.size();
    std::vector<uintptr_t> hits;
    for (auto p = reinterpret_cast<const uint8_t*>(begin); p + len <= reinterpret_cast<const uint8_t*>(end); ++p) {
        p = static_cast<const uint8_t*>(memchr(p, pat.bytes[0], reinterpret_cast<const uint8_t*>(end) - p - len + 1));
        if (!p) break;
        if (MatchAt(p, pat)) hits.push_back(reinterpret_cast<uintptr_t>(p));
    }
    return static_cast<int>(hits.size()) == expected && n < expected ? hits[static_cast<size_t>(n)] : 0;
}

bool Matches(uintptr_t addr, const char* pattern)
{
    Pattern pat = Parse(pattern);
    return addr && MatchAt(reinterpret_cast<const uint8_t*>(addr), pat);
}

uintptr_t Rel32(uintptr_t operand, int instrEndOffset)
{
    int32_t rel;
    memcpy(&rel, reinterpret_cast<void*>(operand), 4);
    return operand + instrEndOffset + rel;
}
}  // namespace ql::scan
