#pragma once
#include <cstdint>
#include <vector>

namespace ql::scan {
// Masked byte search over the main module's .text ("48 8B ?? ..."). Returns 0 when not found or not unique.
uintptr_t Find(const char* pattern);
// Like Find, but for patterns that match a known number of identical copies (e.g. ICF-twin functions);
// returns the n-th (0-based) match only if exactly `expected` matches exist.
uintptr_t FindNth(const char* pattern, int n, int expected);
// Every match (up to `limit`).
std::vector<uintptr_t> FindAll(const char* pattern, size_t limit = 4096);
// Resolves a rel32 operand (call/jmp/rip-relative) whose 4 bytes start at `operand`.
uintptr_t Rel32(uintptr_t operand, int instrEndOffset = 4);
bool Matches(uintptr_t addr, const char* pattern);
}
