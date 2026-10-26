#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace k40 {
// AIDL acknowledgments carry a direction, but no per-request identifier.
// After a timeout, refuse new requests until a session change: a late reply
// must not be mistaken for the acknowledgment of a subsequent request.
class StreamAck {
 public:
  bool Begin(bool start) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_ || quarantined_) return false;
    pending_ = true;
    start_ = start;
    done_ = false;
    success_ = false;
    return true;
  }
  void Reply(bool start, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_ || done_ || start != start_) return;
    done_ = true;
    success_ = success;
    cv_.notify_all();
  }
  // Includes uncertain Binder transport failures; the receiver may have
  // consumed the request even when the caller sees a transaction failure.
  void Reject() {
    std::lock_guard<std::mutex> lock(mutex_);
    quarantined_ = true;
    done_ = true;
    success_ = false;
    cv_.notify_all();
  }
  void SessionChanged() {
    std::lock_guard<std::mutex> lock(mutex_);
    quarantined_ = false;
    if (pending_) {
      done_ = true;
      success_ = false;
    }
    cv_.notify_all();
  }
  bool Wait(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!pending_) return false;
    if (!cv_.wait_for(lock, timeout, [&] { return done_; })) {
      quarantined_ = true;
      success_ = false;
    }
    pending_ = false;
    return success_;
  }
 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool pending_ = false;
  bool start_ = false;
  bool done_ = false;
  bool success_ = false;
  bool quarantined_ = false;
};
}  // namespace k40
