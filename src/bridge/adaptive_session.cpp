#include "adaptive_session.h"
#include "adaptive_capabilities.h"
#include <BluetoothAudioSessionControl.h>
#include <algorithm>
#include <limits>

namespace k40 {
namespace audio = aidl::android::hardware::bluetooth::audio;
using Control = audio::BluetoothAudioSessionControl;
constexpr auto kSession = audio::SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH;

AdaptiveSession::AdaptiveSession() : ack_(std::make_shared<StreamAck>()) {}

AdaptiveSession::~AdaptiveSession() {
  Close();
  if (cookie_ != audio::kObserversCookieUndefined)
    Control::UnregisterControlResultCback(kSession, cookie_);
  // Queued callbacks hold only a weak reference, never a raw this pointer.
  ack_.reset();
}

bool AdaptiveSession::Register() {
  if (cookie_ != audio::kObserversCookieUndefined) return true;
  std::weak_ptr<StreamAck> weak = ack_;
  audio::PortStatusCallbacks callbacks{};
  callbacks.control_result_cb_ = [weak](uint16_t, bool start, audio::BluetoothAudioStatus status) {
    if (auto ack = weak.lock())
      ack->Reply(start, status == audio::BluetoothAudioStatus::SUCCESS);
  };
  callbacks.session_changed_cb_ = [weak](uint16_t) {
    if (auto ack = weak.lock()) ack->SessionChanged();
  };
  callbacks.audio_configuration_changed_cb_ = [weak](uint16_t) {
    // A changed codec requires HAL/DSP reconfiguration, not continuing with
    // the old packed structure. Fail pending control and require session reset.
    if (auto ack = weak.lock()) ack->Reject();
  };
  cookie_ = Control::RegisterControlResultCback(kSession, callbacks);
  return cookie_ != audio::kObserversCookieUndefined;
}

std::optional<LegacyAdaptiveConfig> AdaptiveSession::ReadConfiguration() {
  if (!Control::IsSessionReady(kSession, true)) return std::nullopt;
  const auto config = Control::GetAudioConfig(kSession);
  if (config.getTag() != audio::AudioConfiguration::a2dpConfig) return std::nullopt;
  const auto& codec = config.get<audio::AudioConfiguration::a2dpConfig>();
  if (!IsAdaptiveR2ConfigurationSupported(codec)) return std::nullopt;
  return ConvertAidlR2(codec);
}

bool AdaptiveSession::Open() {
  std::lock_guard<std::mutex> lock(operations_);
  opened_ = Register() && ReadConfiguration().has_value();
  return opened_;
}

bool AdaptiveSession::Ready() {
  std::lock_guard<std::mutex> lock(operations_);
  return ReadConfiguration().has_value();
}

uint16_t AdaptiveSession::SinkLatencyMs() {
  std::lock_guard<std::mutex> lock(operations_);
  audio::PresentationPosition position{};
  if (!ReadConfiguration() || !Control::GetPresentationPosition(kSession, position) ||
      position.remoteDeviceAudioDelayNanos <= 0) return 0;
  const auto ms = position.remoteDeviceAudioDelayNanos / 1000000;
  return static_cast<uint16_t>(std::min<int64_t>(ms, std::numeric_limits<uint16_t>::max()));
}

std::optional<LegacyAdaptiveConfig> AdaptiveSession::Configuration() {
  std::lock_guard<std::mutex> lock(operations_);
  return opened_ ? ReadConfiguration() : std::nullopt;
}

bool AdaptiveSession::Request(bool start, std::chrono::milliseconds timeout) {
  std::lock_guard<std::mutex> lock(operations_);
  if (!opened_ || !ReadConfiguration() || !ack_->Begin(start)) return false;
  // Never hold the acknowledgment mutex across a Binder/session call. A reply
  // may run synchronously or arrive on another thread before this call returns.
  const bool sent = start ? Control::StartStream(kSession, false)
                          : Control::SuspendStream(kSession);
  if (!sent) ack_->Reject();
  return ack_->Wait(timeout);
}

bool AdaptiveSession::Start(std::chrono::milliseconds timeout) {
  return Request(true, timeout);
}
bool AdaptiveSession::Suspend(std::chrono::milliseconds timeout) {
  return Request(false, timeout);
}
void AdaptiveSession::Close() {
  std::lock_guard<std::mutex> lock(operations_);
  if (opened_) Control::StopStream(kSession);
  opened_ = false;
}
}  // namespace k40
