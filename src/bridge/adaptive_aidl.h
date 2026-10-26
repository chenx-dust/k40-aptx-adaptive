#pragma once
#include "adaptive_config.h"
#include <aidl/android/hardware/bluetooth/audio/CodecConfiguration.h>

namespace k40 {
std::optional<LegacyAdaptiveConfig> ConvertAidlR2(
    const aidl::android::hardware::bluetooth::audio::CodecConfiguration& config);
}
