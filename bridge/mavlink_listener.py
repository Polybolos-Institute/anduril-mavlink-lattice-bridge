"""Minimal MAVLink UDP listener → VehiclePose."""

from __future__ import annotations

import time
from typing import Optional

from .lattice_publisher import VehiclePose


class MavlinkListener:
    """
    Prefer pymavlink when installed. Falls back to documenting --sim if import fails.
    """

    def __init__(self, udp_port: int = 14550, bind_host: str = "0.0.0.0") -> None:
        self.udp_port = udp_port
        self.bind_host = bind_host
        self._conn = None
        self._pose = VehiclePose(0.0, 0.0, source="mavlink")

    def connect(self) -> None:
        try:
            from pymavlink import mavutil
        except ImportError as e:
            raise RuntimeError(
                "pymavlink is not installed. Run: pip install pymavlink\n"
                "Or use --sim for sandbox smoke without a vehicle."
            ) from e

        # Listen for SITL / GCS-style UDP (common local pattern).
        self._conn = mavutil.mavlink_connection(
            f"udpin:{self.bind_host}:{self.udp_port}",
            autoreconnect=True,
        )

    def poll(self, timeout_s: float = 0.2) -> Optional[VehiclePose]:
        if self._conn is None:
            raise RuntimeError("call connect() first")
        msg = self._conn.recv_match(blocking=True, timeout=timeout_s)
        if msg is None:
            return None
        mtype = msg.get_type()
        if mtype == "GLOBAL_POSITION_INT":
            self._pose.lat_deg = msg.lat * 1e-7
            self._pose.lon_deg = msg.lon * 1e-7
            self._pose.alt_m = msg.relative_alt * 0.001
            self._pose.heading_deg = (msg.hdg * 0.01) % 360.0 if msg.hdg != 65535 else self._pose.heading_deg
            # vx/vy cm/s → rough ground speed
            vx = getattr(msg, "vx", 0) * 0.01
            vy = getattr(msg, "vy", 0) * 0.01
            self._pose.speed_mps = (vx * vx + vy * vy) ** 0.5
            self._pose.valid = True
            self._pose.source = "GLOBAL_POSITION_INT"
            return self._pose
        if mtype == "GPS_RAW_INT" and getattr(msg, "fix_type", 0) >= 2:
            self._pose.lat_deg = msg.lat * 1e-7
            self._pose.lon_deg = msg.lon * 1e-7
            self._pose.alt_m = msg.alt * 0.001
            self._pose.valid = msg.lat != 0 and msg.lon != 0
            self._pose.source = "GPS_RAW_INT"
            return self._pose if self._pose.valid else None
        if mtype == "HEARTBEAT":
            # Keep last pose; heartbeat only proves link.
            return None
        return None

    def wait_pose(self, max_wait_s: float = 10.0) -> VehiclePose:
        deadline = time.time() + max_wait_s
        while time.time() < deadline:
            p = self.poll(timeout_s=0.5)
            if p is not None and p.valid:
                return p
        raise TimeoutError(
            f"no GLOBAL_POSITION_INT/GPS_RAW_INT on UDP {self.udp_port} within {max_wait_s}s"
        )
