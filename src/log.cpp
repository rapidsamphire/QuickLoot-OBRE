#include "log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <share.h>

namespace ql::log {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}

void Init(const std::wstring& path)
{
    std::lock_guard lock(g_mutex);
    if (!g_file) g_file = _wfsopen(path.c_str(), L"w", _SH_DENYNO);  // readable while the game runs
}

void Write(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    SYSTEMTIME t;
    GetLocalTime(&t);
    std::lock_guard lock(g_mutex);
    if (!g_file) return;
    fprintf(g_file, "[%02d:%02d:%02d.%03d] [T%lu] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
            GetCurrentThreadId(), buf);
    fflush(g_file);
}
}  // namespace ql::log
