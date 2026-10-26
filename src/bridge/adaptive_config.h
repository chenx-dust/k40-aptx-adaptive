#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace k40 {
constexpr int DecodeAidlTtpByte(int8_t value) {
  return static_cast<int>(value) + 128;
}

// SM8250 audio_aptx_ad_config ABI. Not a DSP wire packet: the audio HAL
// transforms this structure again before sending mixer configuration.
struct LegacyAdaptiveConfig {
  uint32_t sampling_rate;
  uint32_t mtu;
  int32_t channel_mode;
  uint32_t sink_buffering[6];
  uint32_t encoder_mode;
  uint8_t ttp[6];
  uint32_t bits_per_sample;
  uint32_t input_mode;
  uint32_t fade_duration;
  uint8_t sink_cap[11];
};
static_assert(sizeof(LegacyAdaptiveConfig) == 72);
static_assert(offsetof(LegacyAdaptiveConfig, encoder_mode) == 36);
static_assert(offsetof(LegacyAdaptiveConfig, bits_per_sample) == 48);
static_assert(offsetof(LegacyAdaptiveConfig, sink_cap) == 60);

// Enum values follow PixelOS AIDL, not the old HIDL bit masks.
// TTP is normalized to the unsigned negotiated byte (AIDL signed value + 128).
struct AdaptiveParameters {
  int sample_rate_hz;
  int peer_mtu;
  int channel_mode;
  int bits_per_sample;
  int mode;
  std::array<int, 6> sink_buffering_ms;
  std::array<int, 6> ttp;
  int input_mode;
  int fade_duration_ms;
  std::vector<uint8_t> sink_cap;
};

// Initial donor-compatible R2 profile only. R2.1 buffer doubling and newer
// extended capability formats require a separately verified adapter.
inline std::optional<LegacyAdaptiveConfig> ConvertR2(const AdaptiveParameters& p) {
  LegacyAdaptiveConfig out{};
  switch (p.sample_rate_hz) {
    case 48000: out.sampling_rate = 1; break;
    case 44100: out.sampling_rate = 2; break;
    case 96000: out.sampling_rate = 3; break;
    default: return std::nullopt;
  }
  if (p.peer_mtu <= 0 || p.peer_mtu > 65535) return std::nullopt;
  if (p.channel_mode != 0 && p.channel_mode != 1 &&
      p.channel_mode != 2 && p.channel_mode != 4) return std::nullopt;
  if (p.bits_per_sample != 16 && p.bits_per_sample != 24 &&
      p.bits_per_sample != 32) return std::nullopt;
  if (p.mode != 0x1000 && p.mode != 0x2000 && p.mode != 0x4000)
    return std::nullopt;
  // This specific MIUI donor forces stereo input. Do not silently mis-map
  // a requested dual-mono input into a different stream layout.
  if (p.input_mode != 0) return std::nullopt;
  if (p.fade_duration_ms < 0 || p.fade_duration_ms > 255 || p.sink_cap.size() != 11)
    return std::nullopt;
  for (size_t i = 0; i < 6; ++i) {
    if (p.sink_buffering_ms[i] < 0 || p.sink_buffering_ms[i] > 255 ||
        p.ttp[i] < 0 || p.ttp[i] > 255) return std::nullopt;
    out.sink_buffering[i] = p.sink_buffering_ms[i];
    out.ttp[i] = static_cast<uint8_t>(p.ttp[i]);
  }
  for (size_t i = 0; i < 6; i += 2)
    if (p.sink_buffering_ms[i] > p.sink_buffering_ms[i + 1]) return std::nullopt;
  out.mtu = p.peer_mtu;
  out.channel_mode = p.channel_mode;
  out.bits_per_sample = p.bits_per_sample;
  out.encoder_mode = p.mode;
  out.input_mode = 0;
  out.fade_duration = p.fade_duration_ms;
  for (size_t i = 0; i < 11; ++i) out.sink_cap[i] = p.sink_cap[i];
  return out;
}
}  // namespace k40
