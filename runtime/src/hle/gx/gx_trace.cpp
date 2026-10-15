// MKW_TRACE_GX=1: a histogram of the GX hooks the game calls, printed with the frame trace (MKW_TRACE_VI=1).
// Only the SMNE01 profile's hook file reports into it. Off by default.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {
std::mutex g_gxTraceMutex;
std::unordered_map<uint32_t, uint32_t> g_gxTraceCounts;
const bool g_gxTraceEnabled = std::getenv("MKW_TRACE_GX") != nullptr;
} // namespace

extern "C" void GxTraceCall(uint32_t address) {
    if (!g_gxTraceEnabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_gxTraceMutex);
    ++g_gxTraceCounts[address];
}

extern "C" void GxDlStatsDump();
extern "C" void GxTraceDump() {
    if (!g_gxTraceEnabled) {
        return;
    }
    std::vector<std::pair<uint32_t, uint32_t>> rows;
    {
        std::lock_guard<std::mutex> lock(g_gxTraceMutex);
        rows.assign(g_gxTraceCounts.begin(), g_gxTraceCounts.end());
        g_gxTraceCounts.clear();
    }
    std::sort(rows.begin(), rows.end());
    std::fprintf(stderr, "[gxtrace] calls since the last dump:");
    for (const auto& row : rows) {
        std::fprintf(stderr, " %08X:%u", row.first, row.second);
    }
    std::fprintf(stderr, "\n");
    GxDlStatsDump();
}
