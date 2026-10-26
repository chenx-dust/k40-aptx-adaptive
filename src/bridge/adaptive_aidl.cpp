#include "adaptive_aidl.h"

namespace k40 {
namespace audio = aidl::android::hardware::bluetooth::audio;

std::optional<LegacyAdaptiveConfig> ConvertAidlR2(const audio::CodecConfiguration& config) {
  using Specific = audio::CodecConfiguration::CodecSpecific;
  if (config.codecType != audio::CodecType::APTX_ADAPTIVE ||
      config.config.getTag() != Specific::aptxAdaptiveConfig) return std::nullopt;
  const auto& ad = config.config.get<Specific::aptxAdaptiveConfig>();
  const auto& buffer = ad.sinkBufferingMs;
  const auto& ttp = ad.ttp;
  auto byte_value = [](auto value) -> int { return static_cast<uint8_t>(value); };
  // Qualcomm's AIDL producer subtracts 128 from each negotiated TTP byte.
  // Restore the original unsigned value for the SM8250 audio HAL ABI.
  auto ttp_value = DecodeAidlTtpByte;
  AdaptiveParameters parameters{
      ad.sampleRateHz,
      config.peerMtu,
      static_cast<int>(ad.channelMode),
      byte_value(ad.bitsPerSample),
      static_cast<int>(ad.aptxMode),
      {byte_value(buffer.minLowLatency), byte_value(buffer.maxLowLatency),
       byte_value(buffer.minHighQuality), byte_value(buffer.maxHighQuality),
       byte_value(buffer.minTws), byte_value(buffer.maxTws)},
      {ttp_value(ttp.lowLowLatency), ttp_value(ttp.highLowLatency),
       ttp_value(ttp.lowHighQuality), ttp_value(ttp.highHighQuality),
       ttp_value(ttp.lowTws), ttp_value(ttp.highTws)},
      static_cast<int>(ad.inputMode),
      ad.inputFadeDurationMs,
      {}};
  parameters.sink_cap.reserve(ad.aptxAdaptiveConfigStream.size());
  for (auto value : ad.aptxAdaptiveConfigStream)
    parameters.sink_cap.push_back(static_cast<uint8_t>(value));
  return ConvertR2(parameters);
}
}  // namespace k40
