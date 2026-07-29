#include "mavlink_udp.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace mlb {
namespace {

constexpr std::uint8_t kMav1Stx = 0xFE;
constexpr std::uint8_t kMav2Stx = 0xFD;
constexpr std::uint32_t kMsgGlobalPositionInt = 33;
constexpr std::size_t kGpiPayload = 28;

std::uint16_t ReadLe16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::int16_t ReadLeI16(const std::uint8_t* p) {
  return static_cast<std::int16_t>(ReadLe16(p));
}

std::uint32_t ReadLe32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t ReadLeI32(const std::uint8_t* p) {
  return static_cast<std::int32_t>(ReadLe32(p));
}

bool DecodeGlobalPositionInt(const std::uint8_t* payload, std::size_t len,
                             VehiclePose* out) {
  if (!out || len < kGpiPayload) {
    return false;
  }
  // time_boot_ms[0..3], lat[4..7], lon[8..11], alt[12..15], relative_alt[16..19],
  // vx[20..21], vy[22..23], vz[24..25], hdg[26..27]
  const std::int32_t lat_e7 = ReadLeI32(payload + 4);
  const std::int32_t lon_e7 = ReadLeI32(payload + 8);
  const std::int32_t alt_mm = ReadLeI32(payload + 12);
  const std::int16_t vx = ReadLeI16(payload + 20);
  const std::int16_t vy = ReadLeI16(payload + 22);
  const std::uint16_t hdg_cdeg = ReadLe16(payload + 26);

  if (lat_e7 == 0 && lon_e7 == 0) {
    return false;
  }

  out->lat_deg = lat_e7 / 1.0e7;
  out->lon_deg = lon_e7 / 1.0e7;
  out->alt_m = alt_mm / 1000.0;
  const double vx_mps = vx / 100.0;
  const double vy_mps = vy / 100.0;
  out->speed_mps = std::hypot(vx_mps, vy_mps);
  if (hdg_cdeg != 65535) {
    out->heading_deg = hdg_cdeg / 100.0;
  }
  out->valid = true;
  out->source = "mavlink_udp";
  return true;
}

}  // namespace

MavlinkUdpListener::MavlinkUdpListener(std::uint16_t port) : port_(port) {}

MavlinkUdpListener::~MavlinkUdpListener() { Stop(); }

bool MavlinkUdpListener::Start() {
  if (started_) {
    return true;
  }

  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    std::cerr << "[mlb] WSAStartup failed\n";
    return false;
  }

  SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) {
    std::cerr << "[mlb] socket failed\n";
    WSACleanup();
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::cerr << "[mlb] bind UDP " << port_ << " failed\n";
    closesocket(s);
    WSACleanup();
    return false;
  }

  u_long nonblock = 1;
  ioctlsocket(s, FIONBIO, &nonblock);

  sock_ = reinterpret_cast<void*>(s);
  started_ = true;
  std::cerr << "[mlb] listening MAVLink UDP on 0.0.0.0:" << port_ << "\n";
  return true;
}

void MavlinkUdpListener::Stop() {
  if (!started_) {
    return;
  }
  SOCKET s = reinterpret_cast<SOCKET>(sock_);
  if (s != INVALID_SOCKET) {
    closesocket(s);
  }
  sock_ = nullptr;
  started_ = false;
  WSACleanup();
}

bool MavlinkUdpListener::Poll(VehiclePose* out, int timeout_ms) {
  if (!started_ || !out) {
    return false;
  }
  SOCKET s = reinterpret_cast<SOCKET>(sock_);

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(s, &readfds);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  const int sel = select(0, &readfds, nullptr, nullptr, &tv);
  if (sel <= 0) {
    return false;
  }

  std::uint8_t packet[2048];
  sockaddr_in from{};
  int fromlen = sizeof(from);
  const int n = recvfrom(s, reinterpret_cast<char*>(packet), sizeof(packet), 0,
                         reinterpret_cast<sockaddr*>(&from), &fromlen);
  if (n <= 0) {
    return false;
  }

  bool updated = false;
  Feed(packet, static_cast<std::size_t>(n), out, &updated);
  return updated;
}

void MavlinkUdpListener::Feed(const std::uint8_t* data, std::size_t len,
                              VehiclePose* out, bool* updated) {
  rx_buf_.append(reinterpret_cast<const char*>(data), len);
  *updated = false;

  while (!rx_buf_.empty()) {
    const auto* buf =
        reinterpret_cast<const std::uint8_t*>(rx_buf_.data());
    const std::size_t n = rx_buf_.size();

    std::size_t start = 0;
    while (start < n && buf[start] != kMav1Stx && buf[start] != kMav2Stx) {
      ++start;
    }
    if (start > 0) {
      rx_buf_.erase(0, start);
      continue;
    }
    if (n < 6) {
      return;
    }

    const std::uint8_t stx = buf[0];
    std::size_t header_len = 0;
    std::size_t payload_len = 0;
    std::size_t crc_len = 2;
    std::uint32_t msgid = 0;

    if (stx == kMav1Stx) {
      // STX LEN SEQ SYSID COMPID MSGID PAYLOAD CRC
      header_len = 6;
      payload_len = buf[1];
      msgid = buf[5];
    } else if (stx == kMav2Stx) {
      // STX LEN INCOMPAT COMPAT SEQ SYSID COMPID MSGID(3) PAYLOAD CRC [sig]
      if (n < 10) {
        return;
      }
      header_len = 10;
      payload_len = buf[1];
      const std::uint8_t incompat = buf[2];
      msgid = static_cast<std::uint32_t>(buf[7]) |
              (static_cast<std::uint32_t>(buf[8]) << 8) |
              (static_cast<std::uint32_t>(buf[9]) << 16);
      if (incompat & 0x01) {
        crc_len = 2 + 13;  // signature
      }
    } else {
      rx_buf_.erase(0, 1);
      continue;
    }

    const std::size_t frame_len = header_len + payload_len + crc_len;
    if (n < frame_len) {
      return;
    }

    if (msgid == kMsgGlobalPositionInt) {
      VehiclePose pose;
      if (DecodeGlobalPositionInt(buf + header_len, payload_len, &pose)) {
        *out = pose;
        *updated = true;
        ++frames_ok_;
      } else {
        ++frames_bad_;
      }
    }

    rx_buf_.erase(0, frame_len);
  }
}

}  // namespace mlb
