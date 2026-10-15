// MKW_TRACE_GX=1: a histogram of the GX hooks the game calls, printed with the frame trace (MKW_TRACE_VI=1).
// Only the SMNE01 profile's hook file reports into it. Off by default.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {
std::mutex g_gxTraceMutex;
std::unordered_map<uint32_t, uint32_t> g_gxTraceCounts;
const bool g_gxTraceEnabled = std::getenv("MKW_TRACE_GX") != nullptr;
} // namespace

extern "C" bool GxTraceFrame();
static std::vector<uint32_t> g_gxTraceSeq;

extern "C" void GxTraceCall(uint32_t address) {
    if (!g_gxTraceEnabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_gxTraceMutex);
    ++g_gxTraceCounts[address];
    if (GxTraceFrame() && g_gxTraceSeq.size() < 200000) {
        g_gxTraceSeq.push_back(address);
    }
}

// The hooks called during the traced frame, in order, run-length compressed. Called at the frame's CopyDisp.
extern "C" void GxTraceFlushSequence() {
    std::vector<uint32_t> seq;
    {
        std::lock_guard<std::mutex> lock(g_gxTraceMutex);
        seq.swap(g_gxTraceSeq);
    }
    if (seq.empty()) {
        return;
    }
    std::fprintf(stderr, "[gxseq] %zu calls:", seq.size());
    size_t i = 0;
    while (i < seq.size()) {
        size_t j = i;
        while (j < seq.size() && seq[j] == seq[i]) {
            ++j;
        }
        std::fprintf(stderr, " %05X", seq[i] & 0xFFFFF);
        if (j - i > 1) {
            std::fprintf(stderr, "x%zu", j - i);
        }
        i = j;
    }
    std::fprintf(stderr, "\n");
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

extern "C" int g_gxFrameCount;

// MKW_TRACE_GX=1 also logs, in order, the state and copy calls of one frame in every 600 (the 300th).
extern "C" bool GxTraceFrame() {
    return g_gxTraceEnabled && (g_gxFrameCount % 600) == 300;
}

extern "C" void GxTraceEvent(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    std::fprintf(stderr, "[gxframe %d] ", g_gxFrameCount);
    std::vfprintf(stderr, format, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}
