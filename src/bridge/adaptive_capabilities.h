#pragma once
#include "adaptive_aidl.h"
#include <aidl/android/hardware/bluetooth/audio/CodecCapabilities.h>

namespace k40 {
aidl::android::hardware::bluetooth::audio::CodecCapabilities AdaptiveR2Capabilities();
bool IsAdaptiveR2ConfigurationSupported(
    const aidl::android::hardware::bluetooth::audio::CodecConfiguration& configuration);
}  // namespace k40
