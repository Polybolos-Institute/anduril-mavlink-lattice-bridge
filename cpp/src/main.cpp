#include "lattice_rest.hpp"
#include "mavlink_udp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

const char* EnvOrEmpty(const char* key) {
  const char* v = std::getenv(key);
  return v ? v : "";
}

void PrintUsage() {
  std::cerr
      << "mavlink_lattice_bridge - MAVLink → Lattice entity door (C++ / WinHTTP)\n"
      << "\n"
      << "Usage:\n"
      << "  mavlink_lattice_bridge --auth-only\n"
      << "  mavlink_lattice_bridge --sim [--once]\n"
      << "  mavlink_lattice_bridge --mavlink-udp [PORT] [--once]\n"
      << "\n"
      << "Env:\n"
      << "  LATTICE_ENDPOINT       host:port\n"
      << "  LATTICE_CLIENT_ID\n"
      << "  LATTICE_CLIENT_SECRET\n"
      << "  LATTICE_ENV_TOKEN      Sandboxes Bearer\n"
      << "  ENTITY_ID              optional (default polybolos-mavlink-ownship)\n";
}

mlb::LatticeRestClient MakeClientFromEnv() {
  mlb::LatticeRestClient client;
  client.SetEndpoint(EnvOrEmpty("LATTICE_ENDPOINT"));
  client.SetCredentials(EnvOrEmpty("LATTICE_CLIENT_ID"),
                        EnvOrEmpty("LATTICE_CLIENT_SECRET"),
                        EnvOrEmpty("LATTICE_ENV_TOKEN"));
  const char* eid = EnvOrEmpty("ENTITY_ID");
  if (eid[0] != '\0') {
    client.SetEntityId(eid);
  }
  return client;
}

}  // namespace

int main(int argc, char** argv) {
  bool auth_only = false;
  bool sim = false;
  bool once = false;
  bool mavlink = false;
  std::uint16_t mav_port = 14550;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      PrintUsage();
      return 0;
    }
    if (a == "--auth-only") {
      auth_only = true;
      continue;
    }
    if (a == "--sim") {
      sim = true;
      continue;
    }
    if (a == "--once") {
      once = true;
      continue;
    }
    if (a == "--mavlink-udp") {
      mavlink = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        mav_port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
      }
      continue;
    }
    std::cerr << "unknown arg: " << a << "\n";
    PrintUsage();
    return 2;
  }

  if (!auth_only && !sim && !mavlink) {
    PrintUsage();
    return 2;
  }

  auto client = MakeClientFromEnv();
  const auto missing = client.MissingConfig();
  if (!missing.empty()) {
    std::cerr << "[mlb] missing env:";
    for (const auto& m : missing) {
      std::cerr << " " << m;
    }
    std::cerr << "\n";
    return 1;
  }

  if (auth_only) {
    if (!client.FetchToken()) {
      std::cerr << "[mlb] auth-only FAILED\n";
      return 1;
    }
    std::cerr << "[mlb] auth-only OK endpoint=" << client.EndpointHost() << "\n";
    return 0;
  }

  if (sim) {
    do {
      const mlb::VehiclePose pose = mlb::MakeSimPose();
      const mlb::PublishResult r = client.PublishPose(pose);
      std::cerr << "[mlb] sim publish HTTP " << r.status_code
                << " ok=" << (r.ok ? "true" : "false") << " id="
                << client.EntityId() << " lat=" << pose.lat_deg
                << " lon=" << pose.lon_deg << "\n";
      if (!r.ok) {
        std::cerr << "[mlb] body=" << r.body << "\n";
        return 1;
      }
      if (once) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(2));
    } while (true);
    return 0;
  }

  mlb::MavlinkUdpListener listener(mav_port);
  if (!listener.Start()) {
    return 1;
  }

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(once ? 30 : 0);
  for (;;) {
    mlb::VehiclePose pose;
    if (listener.Poll(&pose, 500)) {
      const mlb::PublishResult r = client.PublishPose(pose);
      std::cerr << "[mlb] mavlink publish HTTP " << r.status_code
                << " ok=" << (r.ok ? "true" : "false") << " lat=" << pose.lat_deg
                << " lon=" << pose.lon_deg << " frames_ok=" << listener.FramesOk()
                << "\n";
      if (!r.ok) {
        std::cerr << "[mlb] body=" << r.body << "\n";
        if (once) {
          return 1;
        }
      } else if (once) {
        return 0;
      }
    } else if (once && std::chrono::steady_clock::now() >= deadline) {
      std::cerr << "[mlb] --once timed out waiting for GLOBAL_POSITION_INT\n";
      return 1;
    }
  }
}
