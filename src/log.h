#pragma once
#include <string>

namespace ql::log {
void Init(const std::wstring& path);
void Write(const char* fmt, ...);
}

#define QL_LOG(...) ::ql::log::Write(__VA_ARGS__)
