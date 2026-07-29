#pragma once

#include "vehicle_pose.hpp"

#include <cstdint>
#include <string>

namespace mlb {

/// Minimal MAVLink v1/v2 UDP listener for GLOBAL_POSITION_INT (33).
/// No full dialect / no outbound command path.
class MavlinkUdpListener {
 public:
  explicit MavlinkUdpListener(std::uint16_t port = 14550);
  ~MavlinkUdpListener();

  MavlinkUdpListener(const MavlinkUdpListener&) = delete;
  MavlinkUdpListener& operator=(const MavlinkUdpListener&) = delete;

  bool Start();
  void Stop();

  /// Non-blocking poll. Returns true when a fresh pose was decoded.
  bool Poll(VehiclePose* out, int timeout_ms = 50);

  [[nodiscard]] std::uint16_t Port() const { return port_; }
  [[nodiscard]] std::uint64_t FramesOk() const { return frames_ok_; }
  [[nodiscard]] std::uint64_t FramesBad() const { return frames_bad_; }

 private:
  void Feed(const std::uint8_t* data, std::size_t len, VehiclePose* out,
            bool* updated);

  std::uint16_t port_;
  void* sock_{nullptr};  // SOCKET on Win32
  bool started_{false};
  std::string rx_buf_;
  std::uint64_t frames_ok_{0};
  std::uint64_t frames_bad_{0};
};

}  // namespace mlb
