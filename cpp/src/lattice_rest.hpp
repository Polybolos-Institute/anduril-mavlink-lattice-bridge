#pragma once

#include "vehicle_pose.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace mlb {

struct PublishResult {
  int status_code = 0;
  std::string body;
  bool ok = false;
};

/// Thin Lattice REST door: OAuth client-credentials + Sandboxes Bearer + entity PUT.
/// Auth model matches Polybolos HOTL sandbox evidence (WinHTTP on Windows).
class LatticeRestClient {
 public:
  void SetEndpoint(std::string host_port);
  void SetCredentials(std::string client_id, std::string client_secret,
                      std::string env_token);
  void SetEntityId(std::string entity_id);

  [[nodiscard]] std::vector<std::string> MissingConfig() const;
  [[nodiscard]] const std::string& EndpointHost() const { return host_; }
  [[nodiscard]] const std::string& EntityId() const { return entity_id_; }

  bool FetchToken();
  bool EnsureToken();
  [[nodiscard]] bool IsTokenValid() const;

  PublishResult PublishPose(const VehiclePose& pose);

 private:
  std::string BuildEntityJson(const VehiclePose& pose) const;

  std::string host_;
  std::string client_id_;
  std::string client_secret_;
  std::string env_token_;
  std::string entity_id_{"polybolos-mavlink-ownship"};

  std::string access_token_;
  std::chrono::steady_clock::time_point token_expiry_{};
};

VehiclePose MakeSimPose();

}  // namespace mlb
