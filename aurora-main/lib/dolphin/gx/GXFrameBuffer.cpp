#include "gx.hpp"
#include "__gx.h"

#include "../../gfx/tex_copy_conv.hpp"
#include "../../gfx/efb_ram_copy.hpp"
#include "../../gfx/texture.hpp"
#include "../../gx/fifo.hpp"
#include "../../internal.hpp"
#include "../../window.hpp"
#include "../../gfx/clear.hpp"
#include "../../webgpu/gpu.hpp"
#include "../vi/vi_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
struct CopyClearState {
  bool clearColor = false;
  bool clearAlpha = false;
  bool clearDepth = false;
  aurora::Vec4<float> clearColorValue{0.f, 0.f, 0.f, 1.f};
};

CopyClearState get_copy_clear_state(GXBool clear) {
  if (!clear) {
    return {};
  }

  CopyClearState state{
      .clearColor = g_gxState.colorUpdate,
      .clearAlpha = g_gxState.alphaUpdate,
      .clearDepth = g_gxState.depthUpdate,
      .clearColorValue = g_gxState.clearColor,
  };

  if (!aurora::gx::render_target_has_alpha(g_gxState.pixelFmt)) {
    // RGB and depth EFB formats do not have an alpha channel to clear.
    state.clearAlpha = false;
  }

  return state;
}

struct CopySourceRect {
  aurora::gfx::ClipRect clearRect;
  aurora::Vec4<float> sampleRect;
};

CopySourceRect map_texture_copy_source(const aurora::gfx::ClipRect& source, bool renderSpace) {
  if (renderSpace || g_gxState.viewportPolicy == AURORA_VIEWPORT_NATIVE) {
    return {
        .clearRect = source,
        .sampleRect = {static_cast<float>(source.x), static_cast<float>(source.y), static_cast<float>(source.width),
                       static_cast<float>(source.height)},
    };
  }

  const auto [logicalWidth, logicalHeight] = aurora::gx::logical_fb_size();
  const auto [targetWidth, targetHeight] = aurora::gfx::get_render_target_size();
  if (logicalWidth == 0 || logicalHeight == 0 || targetWidth == 0 || targetHeight == 0) {
    return {
        .clearRect = source,
        .sampleRect = {static_cast<float>(source.x), static_cast<float>(source.y), static_cast<float>(source.width),
                       static_cast<float>(source.height)},
    };
  }

  struct MappedEdge {
    float sample;
    int32_t nearest;
  };
  const auto mapEdge = [](int32_t edge, uint32_t logicalSize, uint32_t targetSize) {
    // Round scaled copy edges consistently to avoid seams and off-by-one pixels.
    const int64_t numerator = static_cast<int64_t>(edge) * static_cast<int64_t>(targetSize);
    const int64_t denominator = static_cast<int64_t>(logicalSize);
    const int64_t quotient = numerator / denominator;
    const int64_t remainder = numerator % denominator;
    const int64_t remainderMagnitude = remainder < 0 ? -remainder : remainder;
    const int64_t roundingThreshold = (denominator + 1) / 2;
    const int64_t nearestValue =
        quotient + (remainderMagnitude >= roundingThreshold ? (numerator < 0 ? -1 : 1) : 0);
    return MappedEdge{
        .sample = static_cast<float>(static_cast<double>(numerator) / static_cast<double>(denominator)),
        .nearest = static_cast<int32_t>(nearestValue),
    };
  };
  const auto left = mapEdge(source.x, logicalWidth, targetWidth);
  const auto top = mapEdge(source.y, logicalHeight, targetHeight);
  const auto right = mapEdge(source.x + source.width, logicalWidth, targetWidth);
  const auto bottom = mapEdge(source.y + source.height, logicalHeight, targetHeight);
  return {
      .clearRect =
          {
              .x = left.nearest,
              .y = top.nearest,
              .width = std::max<int32_t>(right.nearest - left.nearest, 1),
              .height = std::max<int32_t>(bottom.nearest - top.nearest, 1),
          },
      .sampleRect = {left.sample, top.sample, right.sample - left.sample, bottom.sample - top.sample},
  };
}





u32 pack_copy_filter_samples(u8 reg, const std::array<std::array<u8, 2>, 12>& samplePattern, size_t first) {
  u32 value = static_cast<u32>(reg) << 24;
  for (size_t i = 0; i < 6; ++i) {
    const size_t sample = first + i;
    const u8 component = samplePattern[sample / 2][sample % 2] & 0x0fu;
    value |= static_cast<u32>(component) << (i * 4);
  }
  return value;
}

u32 pack_copy_filter0(const std::array<u8, 7>& vfilter) {
  return 0x53000000u | (static_cast<u32>(vfilter[0] & 0x3fu) << 0) |
         (static_cast<u32>(vfilter[1] & 0x3fu) << 6) | (static_cast<u32>(vfilter[2] & 0x3fu) << 12) |
         (static_cast<u32>(vfilter[3] & 0x3fu) << 18);
}

u32 pack_copy_filter1(const std::array<u8, 7>& vfilter) {
  return 0x54000000u | (static_cast<u32>(vfilter[4] & 0x3fu) << 0) |
         (static_cast<u32>(vfilter[5] & 0x3fu) << 6) | (static_cast<u32>(vfilter[6] & 0x3fu) << 12);
}

std::array<u32, 3> combined_copy_filter_coefficients(const std::array<u8, 7>& vfilter) {
  if (!g_gxState.copyFilterVf) {
    return {0, 64, 0};
  }

  return {
      static_cast<u32>(vfilter[0]) + static_cast<u32>(vfilter[1]),
      static_cast<u32>(vfilter[2]) + static_cast<u32>(vfilter[3]) + static_cast<u32>(vfilter[4]),
      static_cast<u32>(vfilter[5]) + static_cast<u32>(vfilter[6]),
  };
}

aurora::Vec2<uint32_t> scale_copy_dst(u32 logicalWidth, u32 logicalHeight) {
  if (g_gxState.viewportPolicy == AURORA_VIEWPORT_NATIVE) {
    return {logicalWidth, logicalHeight};
  }

  const auto [logicalFbWidth, logicalFbHeight] = aurora::gx::logical_fb_size();
  const auto [targetWidth, targetHeight] = aurora::gfx::get_render_target_size();
  if (logicalFbWidth == 0 || logicalFbHeight == 0 || targetWidth == 0 || targetHeight == 0) {
    return {logicalWidth, logicalHeight};
  }

  const float scaleX = static_cast<float>(targetWidth) / static_cast<float>(logicalFbWidth);
  const float scaleY = static_cast<float>(targetHeight) / static_cast<float>(logicalFbHeight);
  const auto scaledWidth = std::max<u32>(static_cast<u32>(std::lround(static_cast<float>(logicalWidth) * scaleX)), 1);
  const auto scaledHeight = std::max<u32>(static_cast<u32>(std::lround(static_cast<float>(logicalHeight) * scaleY)), 1);
  return {scaledWidth, scaledHeight};
}

aurora::gfx::TextureHandle create_copy_texture(u32 width, u32 height, GXTexFmt texCopyFmt) {
  if (aurora::gfx::tex_copy_conv::needs_conversion(texCopyFmt)) {
    return aurora::gfx::new_conv_texture(width, height, texCopyFmt, "Copy Conv Texture");
  }

  const auto fmt = texCopyFmt == GX_TF_RGB565 ? GX_TF_RGB565 : GX_TF_RGBA8;
  return aurora::gfx::new_render_texture(width, height, fmt, "Resolved Texture");
}

  // Reuse retired copy targets to avoid unbounded GPU texture allocation.
struct CopyTexturePoolEntry {
  aurora::gx::GXState::CopyTextureKey key;
  u32 scaledWidth = 0;
  u32 scaledHeight = 0;
  aurora::gfx::TextureHandle handle;
};
std::vector<CopyTexturePoolEntry> g_copyTexturePool;
  // Keep only a few reusable copy targets per destination.
constexpr size_t kCopyTexturePoolPerKey = 3;

aurora::gfx::TextureHandle acquire_copy_texture(const aurora::gx::GXState::CopyTextureKey& key, u32 width, u32 height,
                                                GXTexFmt texCopyFmt) {
  size_t sameKey = 0;
  for (auto it = g_copyTexturePool.begin(); it != g_copyTexturePool.end();) {
    if (!(it->key == key)) {
      ++it;
      continue;
    }
    // Discard retired targets when their internal resolution changes.
    if ((it->scaledWidth != width || it->scaledHeight != height) && it->handle.use_count() <= 1) {
      it = g_copyTexturePool.erase(it);
      continue;
    }
    ++sameKey;
    if (it->scaledWidth == width && it->scaledHeight == height && it->handle.use_count() == 1) {
      return it->handle;
    }
    ++it;
  }

  auto handle = create_copy_texture(width, height, texCopyFmt);
  if (sameKey < kCopyTexturePoolPerKey) {
    g_copyTexturePool.push_back({key, width, height, handle});
  }
  return handle;
}

u16 get_num_xfb_lines_internal(u16 efbHeight, u32 iScale) {
  CHECK(efbHeight != 0, "GXGetNumXfbLines requires non-zero EFB height");
  CHECK(iScale != 0, "invalid XFB line scale");

  const u32 count = static_cast<u32>(efbHeight - 1u) * 0x100u;
  u32 realHeight = (count / iScale) + 1u;

  u32 reducedScale = iScale;
  if (reducedScale > 0x80u && reducedScale < 0x100u) {
    while ((reducedScale & 1u) == 0u) {
      reducedScale >>= 1;
    }
    if (reducedScale != 0u && (efbHeight % reducedScale) == 0u) {
      ++realHeight;
    }
  }

  return static_cast<u16>(std::min<u32>(realHeight, 0x400u));
}

u32 y_scale_to_integer(f32 yScale) {
  CHECK(yScale > 0.f, "GX display copy y-scale must be positive");
  return static_cast<u32>(256.f / yScale) & 0x1ffu;
}

} // namespace

namespace aurora::gx {
void prune_copy_texture_pool(const void* dest) noexcept {
  if (dest == nullptr) {
    g_copyTexturePool.clear();
    return;
  }
  std::erase_if(g_copyTexturePool, [dest](const CopyTexturePoolEntry& entry) { return entry.key.dest == dest; });
}
} // namespace aurora::gx

extern "C" {
GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    0,
    0,
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {8, 8, 10, 12, 10, 8, 8},
};
GXRenderModeObj GXNtsc480Int = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    0,
    0,
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {0, 0, 21, 22, 21, 0, 0},
};
GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT,
    704,
    528,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    0,
    0,
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {8, 8, 10, 12, 10, 8, 8},
};
GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_PAL_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    0,
    0,
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {8, 8, 10, 12, 10, 8, 8},
};

void GXAdjustForOverscan(GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver) {
  *rmout = *rmin;
  const auto renderSize = aurora::gfx::get_render_target_size();
  rmout->fbWidth = static_cast<u16>(std::min<uint32_t>(renderSize.x, UINT16_MAX));
  rmout->efbHeight = static_cast<u16>(std::min<uint32_t>(renderSize.y, UINT16_MAX));
  rmout->xfbHeight = static_cast<u16>(std::min<uint32_t>(renderSize.y, UINT16_MAX));
}

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
  g_gxState.dispCopySrc = {left, top, wd, ht};
  GX_WRITE_RAS_REG(0x49000000u | ((static_cast<u32>(top) & 0x3ffu) << 10) | (static_cast<u32>(left) & 0x3ffu));
  GX_WRITE_RAS_REG(0x4a000000u | (((static_cast<u32>(ht) - 1u) * 0x400u) & 0x000ffc00u) |
                   ((static_cast<u32>(wd) - 1u) & 0x3ffu));
}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
  g_gxState.texCopySrc = {left, top, wd, ht};
  g_gxState.texCopySrcRenderSpace = false;
}

void GXSetDispCopyDst(u16 wd, u16 ht) {
  g_gxState.dispCopyDstWidth = wd;
  g_gxState.dispCopyDstHeight = ht;
  GX_WRITE_RAS_REG(0x4d000000u | ((((static_cast<u32>(wd) & 0x7fffu) << 1) >> 5) & 0x3ffu));
}

void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) {
  g_gxState.texCopyFmt = fmt;
  g_gxState.texCopyDstWidth = wd;
  g_gxState.texCopyDstHeight = ht;
  g_gxState.texCopyHalfScale = mipmap != GX_FALSE;
}

void GXSetDispCopyFrame2Field(u32 mode) {
  g_gxState.dispCopyFrame2Field = mode & 3;
}

void GXSetCopyClamp(GXFBClamp clamp) {
  g_gxState.copyClamp = static_cast<GXFBClamp>(static_cast<u32>(clamp) & 3);
}

u32 GXSetDispCopyYScale(f32 vscale) {
  const u32 iScale = y_scale_to_integer(vscale);
  g_gxState.dispCopyYScale = vscale;
  GX_WRITE_RAS_REG(0x4e000000u | iScale);
  __gx->bpSent = 0;
  return get_num_xfb_lines_internal(static_cast<u16>(g_gxState.dispCopySrc.height), iScale);
}

void GXSetCopyClear(GXColor color, u32 depth) {
  // BP 0x4F: clear color R + A
  u32 reg0 = 0;
  SET_REG_FIELD(0, reg0, 8, 0, color.r);
  SET_REG_FIELD(0, reg0, 8, 8, color.a);
  SET_REG_FIELD(0, reg0, 8, 24, 0x4F);
  GX_WRITE_RAS_REG(reg0);

  // BP 0x50: clear color B + G
  u32 reg1 = 0;
  SET_REG_FIELD(0, reg1, 8, 0, color.b);
  SET_REG_FIELD(0, reg1, 8, 8, color.g);
  SET_REG_FIELD(0, reg1, 8, 24, 0x50);
  GX_WRITE_RAS_REG(reg1);

  // BP 0x51: clear Z (24-bit)
  u32 reg2 = 0;
  SET_REG_FIELD(0, reg2, 24, 0, depth);
  SET_REG_FIELD(0, reg2, 8, 24, 0x51);
  GX_WRITE_RAS_REG(reg2);
  __gx->bpSent = 1;
}

void GXSetCopyFilter(GXBool aa, u8 sample_pattern[12][2], GXBool vf, u8 vfilter[7]) {
  g_gxState.copyFilterAa = aa;
  g_gxState.copyFilterVf = vf;
  if (sample_pattern) {
    for (size_t i = 0; i < g_gxState.copyFilterSamplePattern.size(); ++i) {
      g_gxState.copyFilterSamplePattern[i][0] = sample_pattern[i][0];
      g_gxState.copyFilterSamplePattern[i][1] = sample_pattern[i][1];
    }
  }
  if (vfilter) {
    for (size_t i = 0; i < g_gxState.copyFilterVFilter.size(); ++i) {
      g_gxState.copyFilterVFilter[i] = vfilter[i];
    }
  }

  if (!aa) {
    for (auto& sample : g_gxState.copyFilterSamplePattern) {
      sample = {6, 6};
    }
  }
  if (!vf) {
    g_gxState.copyFilterVFilter = {0, 0, 21, 22, 21, 0, 0};
  }

  GX_WRITE_RAS_REG(pack_copy_filter_samples(0x01, g_gxState.copyFilterSamplePattern, 0));
  GX_WRITE_RAS_REG(pack_copy_filter_samples(0x02, g_gxState.copyFilterSamplePattern, 6));
  GX_WRITE_RAS_REG(pack_copy_filter_samples(0x03, g_gxState.copyFilterSamplePattern, 12));
  GX_WRITE_RAS_REG(pack_copy_filter_samples(0x04, g_gxState.copyFilterSamplePattern, 18));
  GX_WRITE_RAS_REG(pack_copy_filter0(g_gxState.copyFilterVFilter));
  GX_WRITE_RAS_REG(pack_copy_filter1(g_gxState.copyFilterVFilter));
  __gx->bpSent = 0;
}

void GXSetDispCopyGamma(GXGamma gamma) {
  g_gxState.dispCopyGamma = static_cast<GXGamma>(static_cast<u32>(gamma) & 3u);
  g_gxState.bpRegCache[0x52] = (g_gxState.bpRegCache[0x52] & ~(3u << 7)) |
                               ((static_cast<u32>(g_gxState.dispCopyGamma) & 3u) << 7);
}

void GXCopyDisp(void* dest, GXBool clear) {
  (void)dest;
  // Finish queued commands before this copy reads live EFB state.
  if (aurora::gx::fifo::get_buffer_size() != 0) {
    aurora::gx::fifo::drain();
  }
  const auto rect = aurora::gx::map_logical_scissor(g_gxState.dispCopySrc);
  const auto logicalDstWidth =
      std::max<u32>(g_gxState.dispCopyDstWidth != 0 ? g_gxState.dispCopyDstWidth : static_cast<u32>(g_gxState.dispCopySrc.width), 1);
  const auto logicalDstHeight =
      std::max<u32>(g_gxState.dispCopyDstHeight != 0 ? g_gxState.dispCopyDstHeight : static_cast<u32>(g_gxState.dispCopySrc.height), 1);
  const auto [dstWidth, dstHeight] = scale_copy_dst(logicalDstWidth, logicalDstHeight);

  if (!g_gxState.displayCopyTexture || g_gxState.displayCopyWidth != dstWidth ||
      g_gxState.displayCopyHeight != dstHeight) {
    g_gxState.displayCopyTexture = aurora::gfx::new_render_texture(dstWidth, dstHeight, GX_TF_RGBA8, "Display Copy");
    g_gxState.displayCopyWidth = dstWidth;
    g_gxState.displayCopyHeight = dstHeight;
  }

  const auto clearState = get_copy_clear_state(clear);
  auto copyFilter = combined_copy_filter_coefficients(g_gxState.copyFilterVFilter);
  if (aurora::g_config.disableCopyFilter) {
    copyFilter = {0, copyFilter[0] + copyFilter[1] + copyFilter[2], 0};
  }
  aurora::gfx::resolve_pass(g_gxState.displayCopyTexture, rect, clearState.clearColor, clearState.clearAlpha,
                            clearState.clearDepth, clearState.clearColorValue, aurora::gx::clear_depth_value(),
                            GX_TF_RGBA8, nullptr, false, &copyFilter, false,
                            static_cast<float>(rect.height) / std::max<float>(g_gxState.dispCopySrc.height, 1.0f),
                            (g_gxState.copyClamp & GX_CLAMP_TOP) != 0,
                            (g_gxState.copyClamp & GX_CLAMP_BOTTOM) != 0);
  aurora::gx::set_display_copy_present_source();
}

void GXCopyTex(void* dest, GXBool clear) {
  // Texture copies must see all earlier draws and state changes.
  if (aurora::gx::fifo::get_buffer_size() != 0) {
    aurora::gx::fifo::drain();
  }
  const auto sourceRect = map_texture_copy_source(g_gxState.texCopySrc, g_gxState.texCopySrcRenderSpace);
  const auto rect = sourceRect.clearRect;
  // Keep guest dimensions for cache identity while preserving scaled GPU detail.
  const auto logicalDstWidth = std::max<u32>(g_gxState.texCopyDstWidth, 1);
  const auto logicalDstHeight = std::max<u32>(g_gxState.texCopyDstHeight, 1);
  const auto [scaledDstWidth, scaledDstHeight] = scale_copy_dst(logicalDstWidth, logicalDstHeight);
  const auto texCopyFmt = g_gxState.texCopyFmt;
  const bool sourceHasAlpha = aurora::gx::render_target_has_alpha(g_gxState.pixelFmt);
  const bool forceOpaqueAlpha = !sourceHasAlpha && !aurora::gx::is_depth_format(texCopyFmt);
  const auto resolveFmt = texCopyFmt;

  const aurora::gx::GXState::CopyTextureKey key{
      .dest = dest,
      .width = logicalDstWidth,
      .height = logicalDstHeight,
      .format = texCopyFmt,
  };
  // Keep one live copy per destination and retire stale GPU textures.
  for (auto cacheIt = g_gxState.copyTextureCache.begin(); cacheIt != g_gxState.copyTextureCache.end();) {
    if (cacheIt->first.dest == dest && !(cacheIt->first == key)) {
      g_gxState.copyTextureCache.erase(cacheIt++);
    } else {
      ++cacheIt;
    }
  }
  auto it = g_gxState.copyTextureCache.find(key);
  if (it == g_gxState.copyTextureCache.end()) {
    auto handle = acquire_copy_texture(key, scaledDstWidth, scaledDstHeight, texCopyFmt);
    it = g_gxState.copyTextureCache.emplace(key, aurora::gx::GXState::CopyTextureRef{.handle = handle, .revision = 0}).first;
  }
  auto& handle = it->second;
  const u32 currentFrame = aurora::gfx::current_frame();
  const bool sampledThisFrame = handle.sampledThisFrame && handle.lastSampledFrame == currentFrame;
  const bool scaledSizeChanged = !handle.handle || handle.handle->size.width != scaledDstWidth ||
                                 handle.handle->size.height != scaledDstHeight;
  auto clearState = get_copy_clear_state(clear);
  if (sampledThisFrame || scaledSizeChanged) {
    const u32 revision = handle.revision;
    const u32 lastProducedFrame = handle.lastProducedFrame;
    handle = aurora::gx::GXState::CopyTextureRef{
        .handle = acquire_copy_texture(key, scaledDstWidth, scaledDstHeight, texCopyFmt),
        .revision = revision,
        .lastProducedFrame = lastProducedFrame,
    };
  }

  const bool alphaUpdate = g_gxState.alphaUpdate && aurora::gx::render_target_has_alpha(g_gxState.pixelFmt);
  if (alphaUpdate && g_gxState.dstAlpha != UINT32_MAX) {
    if (!clear) {
      // TODO: Confirm how this copy should handle alpha without changing the EFB.
    }
    // Clear alpha with a pipeline that matches the pass sample count.
    aurora::gfx::push_draw_command(aurora::gfx::clear::DrawData{
        .pipeline = aurora::gfx::pipeline_ref(aurora::gfx::clear::PipelineConfig{
            .msaaSamples = aurora::gfx::get_sample_count(),
            .clearColor = false,
            .clearAlpha = true,
            .clearDepth = false,
        }),
        .color = wgpu::Color{0.f, 0.f, 0.f, g_gxState.dstAlpha / 255.f},
    });
  }
  if (aurora::gx::render_target_has_alpha(g_gxState.pixelFmt)) {
    clearState.clearAlpha = clear && alphaUpdate;
  }
  const auto copyFilter = combined_copy_filter_coefficients(g_gxState.copyFilterVFilter);
  // Skip only recurring color copies so one-shot copies are never lost.
  const bool producedConsecutively = handle.revision != 0 && currentFrame - handle.lastProducedFrame <= 1;
  const bool persistentCopy = !aurora::gx::is_depth_format(texCopyFmt) && !producedConsecutively;
  aurora::gfx::resolve_pass(handle.handle, rect, clearState.clearColor, clearState.clearAlpha, clearState.clearDepth,
                            clearState.clearColorValue, aurora::gx::clear_depth_value(), resolveFmt,
                            &sourceRect.sampleRect, g_gxState.texCopyHalfScale, &copyFilter, forceOpaqueAlpha,
                            sourceRect.sampleRect.w() / std::max<float>(g_gxState.texCopySrc.height, 1.0f),
                            (g_gxState.copyClamp & GX_CLAMP_TOP) != 0,
                            (g_gxState.copyClamp & GX_CLAMP_BOTTOM) != 0, persistentCopy);
  ++handle.revision;
  handle.lastProducedFrame = currentFrame;
  handle.width = logicalDstWidth;
  handle.height = logicalDstHeight;
  handle.format = texCopyFmt;
  handle.dataSize = GXGetTexBufferSize(static_cast<u16>(logicalDstWidth), static_cast<u16>(logicalDstHeight), texCopyFmt, GX_FALSE, 0);
  aurora::gx::notify_copy_texture_created();
  g_gxState.copyTextures[dest] = handle;
  // Keep the GPU copy and download it only if guest code reads the destination.
  aurora::gfx::efb_ram::schedule(dest, logicalDstWidth, logicalDstHeight, texCopyFmt, handle.handle);
}

void GXCopyTexToRegion(void* dest, const void* base, u16 baseWidth, u16 baseHeight, GXTexFmt baseFormat, u16 dstX,
                       u16 dstY, GXBool clear) {
  if (aurora::gx::fifo::get_buffer_size() != 0) {
    aurora::gx::fifo::drain();
  }
  const auto sourceRect = map_texture_copy_source(g_gxState.texCopySrc, g_gxState.texCopySrcRenderSpace);
  const auto rect = sourceRect.clearRect;
  const auto texCopyFmt = g_gxState.texCopyFmt;
  const u32 halfShift = g_gxState.texCopyHalfScale ? 1u : 0u;
  const u32 copyWidth = std::max<u32>(static_cast<u32>(g_gxState.texCopySrc.width) >> halfShift, 1);
  const u32 copyHeight = std::max<u32>(static_cast<u32>(g_gxState.texCopySrc.height) >> halfShift, 1);

  // The texture keeps one GPU image at internal resolution; texel positions scale with it.
  const auto [scaledBaseWidth, scaledBaseHeight] = scale_copy_dst(baseWidth, baseHeight);
  const double scaleX = static_cast<double>(scaledBaseWidth) / static_cast<double>(std::max<u16>(baseWidth, 1));
  const double scaleY = static_cast<double>(scaledBaseHeight) / static_cast<double>(std::max<u16>(baseHeight, 1));
  const auto scaled = [](u32 value, double scale) { return static_cast<int32_t>(std::lround(value * scale)); };
  const int32_t left = scaled(dstX, scaleX);
  const int32_t top = scaled(dstY, scaleY);
  const int32_t right = std::min<int32_t>(scaled(dstX + copyWidth, scaleX), static_cast<int32_t>(scaledBaseWidth));
  const int32_t bottom = std::min<int32_t>(scaled(dstY + copyHeight, scaleY), static_cast<int32_t>(scaledBaseHeight));
  if (right <= left || bottom <= top) {
    return;
  }

  // The texture's GPU image: filled from its RAM contents, then updated in place by each copy. The RAM
  // texture is resolved on every copy: the static texture cache hands back the same upload while the
  // guest has not written that memory, and a new one once it has (a game loading another tileset to the
  // same address, e.g. after a pipe). A new upload means the GPU image is refilled from it.
  const aurora::gx::GXState::CopyTextureKey baseKey{
      .dest = base,
      .width = baseWidth,
      .height = baseHeight,
      .format = baseFormat,
  };
  GXTexObj ramTexture;
  GXInitTexObj(&ramTexture, base, baseWidth, baseHeight, baseFormat, GX_CLAMP, GX_CLAMP, GX_FALSE);
  // No texture-object identity, so the lookup goes by address and contents and adds no per-call entry.
  reinterpret_cast<GXTexObj_*>(&ramTexture)->texObjId = 0;
  const auto ramContents = aurora::gx::resolve_ram_texture(*reinterpret_cast<const GXTexObj_*>(&ramTexture));
  static absl::flat_hash_map<aurora::gx::GXState::CopyTextureKey, aurora::gfx::TextureHandle> s_regionSources;
  aurora::gfx::TextureHandle initialContents;
  auto baseIt = g_gxState.copyTextureCache.find(baseKey);
  if (baseIt == g_gxState.copyTextureCache.end() || !baseIt->second.handle ||
      baseIt->second.handle->size.width != scaledBaseWidth || baseIt->second.handle->size.height != scaledBaseHeight) {
    auto handle = aurora::gfx::new_render_texture(scaledBaseWidth, scaledBaseHeight, GX_TF_RGBA8,
                                                  "GX texture updated by sub-rectangle copies");
    baseIt = g_gxState.copyTextureCache.insert_or_assign(baseKey, aurora::gx::GXState::CopyTextureRef{.handle = handle})
                 .first;
    initialContents = ramContents;
  } else if (const auto source = s_regionSources.find(baseKey);
             source == s_regionSources.end() || source->second != ramContents) {
    initialContents = ramContents;
  }
  if (initialContents) {
    s_regionSources.insert_or_assign(baseKey, initialContents);  }

  // The copied pixels, converted to the copy format exactly as a whole-texture copy would be.
  const aurora::gx::GXState::CopyTextureKey pieceKey{
      .dest = dest,
      .width = copyWidth,
      .height = copyHeight,
      .format = texCopyFmt,
  };
  const auto [scaledCopyWidth, scaledCopyHeight] = scale_copy_dst(copyWidth, copyHeight);
  auto piece = acquire_copy_texture(pieceKey, scaledCopyWidth, scaledCopyHeight, texCopyFmt);

  const bool sourceHasAlpha = aurora::gx::render_target_has_alpha(g_gxState.pixelFmt);
  const bool forceOpaqueAlpha = !sourceHasAlpha && !aurora::gx::is_depth_format(texCopyFmt);
  auto clearState = get_copy_clear_state(clear);
  const bool alphaUpdate = g_gxState.alphaUpdate && aurora::gx::render_target_has_alpha(g_gxState.pixelFmt);
  if (alphaUpdate && g_gxState.dstAlpha != UINT32_MAX) {
    aurora::gfx::push_draw_command(aurora::gfx::clear::DrawData{
        .pipeline = aurora::gfx::pipeline_ref(aurora::gfx::clear::PipelineConfig{
            .msaaSamples = aurora::gfx::get_sample_count(),
            .clearColor = false,
            .clearAlpha = true,
            .clearDepth = false,
        }),
        .color = wgpu::Color{0.f, 0.f, 0.f, g_gxState.dstAlpha / 255.f},
    });
  }
  if (aurora::gx::render_target_has_alpha(g_gxState.pixelFmt)) {
    clearState.clearAlpha = clear && alphaUpdate;
  }
  const auto copyFilter = combined_copy_filter_coefficients(g_gxState.copyFilterVFilter);
  // Room for the resolve and the two blits below, so none of them splits the staging buffer.
  aurora::gfx::ensure_staging_space({0, 4 * aurora::gfx::staging_uniform_bytes(48), 0, 0});
  aurora::gfx::resolve_pass(piece, rect, clearState.clearColor, clearState.clearAlpha, clearState.clearDepth,
                            clearState.clearColorValue, aurora::gx::clear_depth_value(), texCopyFmt,
                            &sourceRect.sampleRect, g_gxState.texCopyHalfScale, &copyFilter, forceOpaqueAlpha,
                            sourceRect.sampleRect.w() / std::max<float>(g_gxState.texCopySrc.height, 1.0f),
                            (g_gxState.copyClamp & GX_CLAMP_TOP) != 0, (g_gxState.copyClamp & GX_CLAMP_BOTTOM) != 0,
                            /*persistentCopy=*/true);
  auto& baseRef = baseIt->second;
  if (initialContents) {
    aurora::gfx::queue_post_resolve_blit(initialContents, baseRef.handle,
                                         {0, 0, static_cast<int32_t>(scaledBaseWidth),
                                          static_cast<int32_t>(scaledBaseHeight)});
  }
  aurora::gfx::queue_post_resolve_blit(piece, baseRef.handle, {left, top, right - left, bottom - top});

  ++baseRef.revision;
  baseRef.lastProducedFrame = aurora::gfx::current_frame();
  baseRef.width = baseWidth;
  baseRef.height = baseHeight;
  baseRef.format = baseFormat;
  baseRef.dataSize = GXGetTexBufferSize(baseWidth, baseHeight, baseFormat, GX_FALSE, 0);
  g_gxState.copyTextures[base] = baseRef;
  aurora::gx::notify_copy_texture_created();
}

void GXClearBoundingBox() {
  g_gxState.boundingBox = {1023, 0, 1023, 0};
  GX_WRITE_RAS_REG(0x550003FFu);
  GX_WRITE_RAS_REG(0x560003FFu);
  __gx->bpSent = 0;
}

void GXReadBoundingBox(u16* left, u16* right, u16* top, u16* bottom) {
  if (left) {
    *left = g_gxState.boundingBox[0];
  }
  if (right) {
    *right = g_gxState.boundingBox[1];
  }
  if (top) {
    *top = g_gxState.boundingBox[2];
  }
  if (bottom) {
    *bottom = g_gxState.boundingBox[3];
  }
}

u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
  return get_num_xfb_lines_internal(efbHeight, y_scale_to_integer(yScale));
}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
  CHECK(efbHeight != 0, "GXGetYScaleFactor requires non-zero EFB height");
  CHECK(xfbHeight != 0 && xfbHeight <= 1024, "GXGetYScaleFactor requires 1..1024 XFB lines");

  u32 targetHeight = xfbHeight;
  f32 yScale = static_cast<f32>(targetHeight) / static_cast<f32>(efbHeight);
  u16 realHeight = GXGetNumXfbLines(efbHeight, yScale);

  while (realHeight > xfbHeight && targetHeight > 1) {
    --targetHeight;
    yScale = static_cast<f32>(targetHeight) / static_cast<f32>(efbHeight);
    realHeight = GXGetNumXfbLines(efbHeight, yScale);
  }

  f32 resultScale = yScale;
  while (realHeight < xfbHeight && targetHeight < 1024) {
    resultScale = yScale;
    ++targetHeight;
    yScale = static_cast<f32>(targetHeight) / static_cast<f32>(efbHeight);
    realHeight = GXGetNumXfbLines(efbHeight, yScale);
  }

  return resultScale;
}

}
