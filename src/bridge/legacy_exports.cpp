#include "adaptive_session.h"
#include <android-base/properties.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <mutex>

// Dedicated Adaptive-only bridge. This is not yet a drop-in replacement for
// btaudio_offload_if.so: routing and other-codec handling must be integrated.
namespace {
// Private stock K40 ABI value; Android 17 audio policy uses 0x27000000 instead.
constexpr uint32_t kLegacyAdaptiveFormat = 0x25000000;
constexpr auto kControlTimeout = std::chrono::milliseconds(3000);
struct Bridge {
  std::mutex mutex;
  k40::AdaptiveSession session;
  bool connected = false;
  bool suspended = false;
};
Bridge& State() {
  static Bridge state;
  return state;
}
int Start() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.connected) return -ENOTCONN;
  if (state.suspended) return -EBUSY;
  // Open may precede AIDL session creation. Recheck at playback start.
  if (!state.session.Open()) return -ENOTSUP;
  return state.session.Start(kControlTimeout) ? 0 : -EIO;
}
int Suspend() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.connected) return -ENOTCONN;
  state.suspended = true;
  return state.session.Suspend(kControlTimeout) ? 0 : -EIO;
}
int Stop() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.connected) return -ENOTCONN;
  // The stock ABI's stop operation suspends A2DP and waits for its ACK;
  // it does not end the AIDL session or unregister the observer.
  return state.session.Suspend(kControlTimeout) ? 0 : -EIO;
}
}  // namespace

extern "C" {
void bt_audio_pre_init() { (void)State(); }
int audio_stream_open() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.connected = true;
  state.suspended = false;
  // The original bridge returns success here even before a session is ready.
  // Readiness and configuration are checked again before starting the DSP.
  (void)state.session.Open();
  return 0;
}
int audio_stream_close() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.session.Close();
  state.connected = false;
  state.suspended = false;
  return 0;
}
int audio_start_stream() { return Start(); }
int audio_stream_start() { return Start(); }
int audio_suspend_stream() { return Suspend(); }
int audio_stream_suspend() { return Suspend(); }
int audio_stop_stream() { return Stop(); }
int audio_stream_stop() { return Stop(); }
int audio_check_a2dp_ready() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.connected && !state.suspended && state.session.Ready();
}
void clear_a2dpsuspend_flag() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.suspended = false;
}
void audio_handoff_triggered() {
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  // Stock handoff resets local state without sending a transport request.
  state.suspended = false;
}
int audio_is_scrambling_enabled() {
  return android::base::GetProperty("persist.vendor.qcom.bluetooth.scram.enabled", "false") ==
         "true";
}
void* audio_get_codec_config(uint8_t* multicast, uint8_t* devices, uint32_t* format) {
  if (multicast) *multicast = 0;
  if (devices) *devices = 0;
  if (format) *format = 0;  // AUDIO_FORMAT_DEFAULT on failure, never stale Adaptive.
  if (!multicast || !devices || !format) return nullptr;
  auto& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.connected) return nullptr;
  const auto config = state.session.Configuration();
  if (!config) return nullptr;
  // Stock HAL consumes the returned structure synchronously. A different
  // caller cannot overwrite it while the HAL translates it to DSP parameters.
  thread_local k40::LegacyAdaptiveConfig snapshot{};
  snapshot = *config;
  *devices = 1;
  *format = kLegacyAdaptiveFormat;
  return &snapshot;
}
uint16_t audio_sink_get_a2dp_latency() { return State().session.SinkLatencyMs(); }
uint16_t audio_get_a2dp_sink_latency() { return State().session.SinkLatencyMs(); }
}  // extern "C"
