#pragma once
#include <cstdint>

namespace theme {
// Palette sampled from the reference photos: dark charcoal chrome, white tiles,
// Xbox-green selection, slate keys.
inline constexpr uint32_t kBg          = 0xFF20242A;
inline constexpr uint32_t kTopBarBg    = 0xFF181C21;
inline constexpr uint32_t kFieldBg     = 0xFF121519;
inline constexpr uint32_t kFieldBorder = 0xFF4A525C;
inline constexpr uint32_t kText        = 0xFFFFFFFF;
inline constexpr uint32_t kTextDim     = 0xFF9AA3AD;
inline constexpr uint32_t kGreen       = 0xFF107C10;
inline constexpr uint32_t kTileBg      = 0xFFFFFFFF;
inline constexpr uint32_t kTileText    = 0xFF1A1A1A;
inline constexpr uint32_t kTileUrl     = 0xFF6E6E6E;
inline constexpr uint32_t kKeyBg       = 0xFF3A4048;
inline constexpr uint32_t kPageBg      = 0xFFFFFFFF;
inline constexpr uint32_t kPageText    = 0xFF1A1A1A;
inline constexpr uint32_t kLink        = 0xFF0066CC;
inline constexpr uint32_t kHintA       = 0xFF3FB950;
inline constexpr uint32_t kHintB       = 0xFFDA4A3C;
inline constexpr uint32_t kHintX       = 0xFF3B7BEF;
inline constexpr uint32_t kHintY       = 0xFFCE9B00;

// Geometry, 1280x720.
inline constexpr float kTopBarH = 56.0f;
inline constexpr float kKeyY    = 412.0f;  // keyboard block top
inline constexpr float kKeyH    = 50.0f;
inline constexpr float kKeyGap  = 6.0f;
inline constexpr float kSugY    = 84.0f;   // suggestion tiles top (edit mode)
inline constexpr float kSugH    = 136.0f;
} // namespace theme
