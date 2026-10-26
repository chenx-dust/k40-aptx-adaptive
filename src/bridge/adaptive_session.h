#pragma once
#include "adaptive_aidl.h"
#include "stream_ack.h"
#include <memory>
#include <mutex>

namespace k40 {
// Lives in the audio HAL process and shares its libbluetooth_audio_session_aidl.
// Own one instance for the lifetime of the bridge; Close does not discard
// timeout quarantine, since AIDL has no request IDs for stale acknowledgments.
class AdaptiveSession {
 public:
  AdaptiveSession();
  ~AdaptiveSession();
  AdaptiveSession(const AdaptiveSession&) = delete;
  AdaptiveSession& operator=(const AdaptiveSession&) = delete;
  bool Open();
  bool Ready();
  uint16_t SinkLatencyMs();
  bool Start(std::chrono::milliseconds timeout);
  bool Suspend(std::chrono::milliseconds timeout);
  void Close();
  std::optional<LegacyAdaptiveConfig> Configuration();
 private:
  bool Register();
  bool Request(bool start, std::chrono::milliseconds timeout);
  std::optional<LegacyAdaptiveConfig> ReadConfiguration();
  std::mutex operations_;
  std::shared_ptr<StreamAck> ack_;
  uint16_t cookie_ = 0;
  bool opened_ = false;
};
}  // namespace k40
