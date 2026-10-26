#include "adaptive_capabilities.h"

namespace k40 {
namespace audio = aidl::android::hardware::bluetooth::audio;
audio::CodecCapabilities AdaptiveR2Capabilities() {
  audio::AptxAdaptiveCapabilities adaptive{};
  // Qualcomm's R2 offload profile (a2dp_aptx_adaptive_offload_caps).
  // 44.1 kHz belongs to its later R2.2 profile; do not advertise that profile
  // just because the old audio ABI has a numeric representation for the rate.
  adaptive.sampleRateHz = {48000, 96000};
  adaptive.bitsPerSample = {24};
  adaptive.channelMode = {audio::AptxAdaptiveChannelMode::JOINT_STEREO,
                          audio::AptxAdaptiveChannelMode::MONO,
                          audio::AptxAdaptiveChannelMode::DUAL_MONO,
                          audio::AptxAdaptiveChannelMode::TWS_STEREO};
  adaptive.aptxMode = {audio::AptxMode::HIGH_QUALITY, audio::AptxMode::LOW_LATENCY,
                       audio::AptxMode::ULTRA_LOW_LATENCY};
  adaptive.sinkBufferingMs = {20, 50, 20, 50, 20, 50};
  adaptive.ttp = {-48, -28, -28, -28, 127, 127};
  adaptive.inputMode = audio::AptxAdaptiveInputMode::STEREO;
  // Negotiated sink capabilities belong to the session configuration. No
  // fabricated sink capability bytes are supplied in this source capability.
  audio::CodecCapabilities result{};
  result.codecType = audio::CodecType::APTX_ADAPTIVE;
  result.capabilities.set<audio::CodecCapabilities::Capabilities::aptxAdaptiveCapabilities>(adaptive);
  return result;
}

bool IsAdaptiveR2ConfigurationSupported(const audio::CodecConfiguration& config) {
  const auto converted = ConvertAidlR2(config);
  return converted && (converted->sampling_rate == 1 || converted->sampling_rate == 3) &&
         converted->bits_per_sample == 24;
}
}  // namespace k40
