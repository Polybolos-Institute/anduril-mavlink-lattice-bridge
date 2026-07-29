#pragma once

#include <string>

namespace mlb {

struct VehiclePose {
  double lat_deg = 0.0;
  double lon_deg = 0.0;
  double alt_m = 0.0;
  double heading_deg = 0.0;
  double speed_mps = 0.0;
  bool valid = false;
  std::string source = "unknown";
};

}  // namespace mlb
