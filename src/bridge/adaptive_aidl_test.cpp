#include "adaptive_aidl.h"
#include "adaptive_capabilities.h"
#include <gtest/gtest.h>
#include <BluetoothAudioCodecs.h>

namespace audio = aidl::android::hardware::bluetooth::audio;

static audio::CodecConfiguration Fixture() {
  audio::AptxAdaptiveConfiguration adaptive;
  adaptive.sampleRateHz = 48000;
  adaptive.channelMode = audio::AptxAdaptiveChannelMode::JOINT_STEREO;
  adaptive.bitsPerSample = 24;
  adaptive.aptxMode = audio::AptxMode::HIGH_QUALITY;
  adaptive.sinkBufferingMs.minLowLatency = 20;
  adaptive.sinkBufferingMs.maxLowLatency = 50;
  adaptive.sinkBufferingMs.minHighQuality = 20;
  adaptive.sinkBufferingMs.maxHighQuality = 50;
  adaptive.sinkBufferingMs.minTws = 20;
  adaptive.sinkBufferingMs.maxTws = 50;
  adaptive.ttp = {-128, -1, 0, 1, 126, 127};
  adaptive.inputMode = audio::AptxAdaptiveInputMode::STEREO;
  adaptive.inputFadeDurationMs = 255;
  adaptive.aptxAdaptiveConfigStream.resize(11);
  adaptive.aptxAdaptiveConfigStream[0] = static_cast<int8_t>(0xff);
  audio::CodecConfiguration config;
  config.codecType = audio::CodecType::APTX_ADAPTIVE;
  config.peerMtu = 1000;
  config.config.set<audio::CodecConfiguration::CodecSpecific::aptxAdaptiveConfig>(adaptive);
  return config;
}

TEST(AdaptiveAidl, RestoresTtpBiasAndPreservesOpaqueCapabilityBits) {
  auto converted = k40::ConvertAidlR2(Fixture());
  ASSERT_TRUE(converted);
  EXPECT_EQ(converted->sampling_rate, 1u);
  const uint8_t expected[] = {0, 127, 128, 129, 254, 255};
  for (size_t i = 0; i < 6; ++i) EXPECT_EQ(converted->ttp[i], expected[i]);
  EXPECT_EQ(converted->sink_cap[0], 255u);
  EXPECT_EQ(converted->mtu, 1000u);
}

TEST(AdaptiveAidl, OnlyAdaptiveIsOffloadedAndSoftwarePcmRemainsAvailable) {
  const auto source = audio::SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
  const auto caps = audio::BluetoothAudioCodecs::GetA2dpOffloadCodecCapabilities(source);
  ASSERT_EQ(caps.size(), 1u);
  EXPECT_EQ(caps[0].codecType, audio::CodecType::APTX_ADAPTIVE);
  EXPECT_TRUE(audio::BluetoothAudioCodecs::IsOffloadCodecConfigurationValid(source, Fixture()));
  auto config = Fixture();
  for (auto codec : {audio::CodecType::SBC, audio::CodecType::AAC, audio::CodecType::APTX,
                     audio::CodecType::APTX_HD, audio::CodecType::LDAC, audio::CodecType::OPUS}) {
    config.codecType = codec;
    EXPECT_FALSE(audio::BluetoothAudioCodecs::IsOffloadCodecConfigurationValid(source, config));
  }
  EXPECT_FALSE(audio::BluetoothAudioCodecs::GetSoftwarePcmCapabilities().empty());
  const auto decoder = audio::SessionType::A2DP_HARDWARE_OFFLOAD_DECODING_DATAPATH;
  EXPECT_FALSE(audio::BluetoothAudioCodecs::IsOffloadCodecConfigurationValid(decoder, Fixture()));
}

TEST(AdaptiveAidl, RejectsWrongCodecOrUnionBeforeAccessingPayload) {
  auto config = Fixture();
  config.codecType = audio::CodecType::APTX_HD;
  EXPECT_FALSE(k40::ConvertAidlR2(config));
  config.codecType = audio::CodecType::APTX_ADAPTIVE;
  config.config = {};
  EXPECT_FALSE(k40::ConvertAidlR2(config));
}

TEST(AdaptiveAidl, RejectsTruncatedCapabilities) {
  auto config = Fixture();
  config.config.get<audio::CodecConfiguration::CodecSpecific::aptxAdaptiveConfig>()
      .aptxAdaptiveConfigStream.resize(10);
  EXPECT_FALSE(k40::ConvertAidlR2(config));
}

TEST(AdaptiveAidl, R2ProfileDoesNotAdvertiseR22SamplingRate) {
  const auto capabilities = k40::AdaptiveR2Capabilities();
  ASSERT_EQ(capabilities.codecType, audio::CodecType::APTX_ADAPTIVE);
  const auto& adaptive = capabilities.capabilities
      .get<audio::CodecCapabilities::Capabilities::aptxAdaptiveCapabilities>();
  EXPECT_EQ(adaptive.sampleRateHz, (std::vector<int32_t>{48000, 96000}));
  auto config = Fixture();
  EXPECT_TRUE(k40::IsAdaptiveR2ConfigurationSupported(config));
  auto& parameters = config.config
      .get<audio::CodecConfiguration::CodecSpecific::aptxAdaptiveConfig>();
  parameters.sampleRateHz = 96000;
  EXPECT_TRUE(k40::IsAdaptiveR2ConfigurationSupported(config));
  parameters.sampleRateHz = 44100;
  EXPECT_FALSE(k40::IsAdaptiveR2ConfigurationSupported(config));
  parameters.sampleRateHz = 48000;
  parameters.aptxAdaptiveConfigStream.clear();
  EXPECT_FALSE(k40::IsAdaptiveR2ConfigurationSupported(config));
}
