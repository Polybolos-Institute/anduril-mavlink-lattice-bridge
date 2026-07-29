"""CLI: python -m bridge [--sim|--mavlink-udp PORT|--auth-only]."""

from __future__ import annotations

import argparse
import os
import sys
import time

from dotenv import load_dotenv

from .lattice_publisher import LatticePublisher, sim_pose
from .mavlink_listener import MavlinkListener


def main(argv: list[str] | None = None) -> int:
    load_dotenv()
    p = argparse.ArgumentParser(
        description="Polybolos MAVLink ↔ Lattice door (no C2 core)"
    )
    p.add_argument("--sim", action="store_true", help="Publish synthetic Dallas track")
    p.add_argument("--once", action="store_true", help="Single publish then exit")
    p.add_argument("--auth-only", action="store_true", help="OAuth smoke only")
    p.add_argument(
        "--mavlink-udp",
        type=int,
        default=int(os.environ.get("MAVLINK_UDP_PORT", "14550")),
        help="UDP port to listen for MAVLink (default 14550)",
    )
    p.add_argument(
        "--hz",
        type=float,
        default=float(os.environ.get("PUBLISH_HZ", "2")),
        help="Publish rate when looping",
    )
    args = p.parse_args(argv)

    pub = LatticePublisher()
    missing = pub.missing_config()
    if missing:
        print("Missing config:", ", ".join(missing), file=sys.stderr)
        print("Copy .env.example → .env and fill Lattice sandbox credentials.", file=sys.stderr)
        return 2

    if args.auth_only:
        info = pub.auth_only()
        print("AUTH_OK", info["endpoint"], info["token_prefix"])
        return 0

    if args.sim:
        return _run_sim(pub, once=args.once, hz=args.hz)

    return _run_mavlink(pub, udp_port=args.mavlink_udp, once=args.once, hz=args.hz)


def _run_sim(pub: LatticePublisher, once: bool, hz: float) -> int:
    period = 1.0 / max(hz, 0.1)
    while True:
        pose = sim_pose()
        r = pub.publish_pose(pose)
        print(
            f"PUBLISH sim lat={pose.lat_deg:.6f} lon={pose.lon_deg:.6f} "
            f"http={r.status_code}"
        )
        if not r.ok:
            print(r.text[:500], file=sys.stderr)
            return 1
        if once:
            return 0
        time.sleep(period)


def _run_mavlink(
    pub: LatticePublisher, udp_port: int, once: bool, hz: float
) -> int:
    listener = MavlinkListener(udp_port=udp_port)
    print(f"Listening MAVLink UDP {udp_port} ...")
    listener.connect()
    period = 1.0 / max(hz, 0.1)
    last_pub = 0.0
    while True:
        pose = listener.poll(timeout_s=0.5)
        if pose is None or not pose.valid:
            continue
        now = time.time()
        if now - last_pub < period and not once:
            continue
        r = pub.publish_pose(pose)
        last_pub = now
        print(
            f"PUBLISH {pose.source} lat={pose.lat_deg:.6f} lon={pose.lon_deg:.6f} "
            f"http={r.status_code}"
        )
        if not r.ok:
            print(r.text[:500], file=sys.stderr)
            return 1
        if once:
            return 0


if __name__ == "__main__":
    raise SystemExit(main())
