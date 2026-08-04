# Anduril Lattice - MAVLink bridge (door only)

## Status & recognition (factual)

> Independent Polybolos Institute sample (not an Anduril product).  
> OASW(SO/LIC) Jul 2026 **Selected** (technically meritorious; under evaluation/consideration).  
> AFRL Apr 2026: RQ portfolio share (Col Rondeau) + Control Science Center exchange (Weintraub; “state of the art” / partnership / SBIR language in correspondence). Attributed dialogue.  
> TRL 5 Decision-C2 lineage · Lattice sandbox / interop sample · Inquiries: mark.brown@polybolos.org · CAGE 1AVY9 · UEI RUSHH9B2UQV3

Standalone **door** that listens to MAVLink vehicle telemetry and publishes
Anduril Lattice World Model entities. No C2 core, no ROE, no engagement authority.

Built by [Polybolos Institute](https://www.polybolos.org) for Lattice sandbox
interoperability demos. Complements (does not replace) HOTL / sealed Core.
**Independent sample - not an Anduril product.**

| Direction | Behavior |
|-----------|----------|
| **Up** | MAVLink `GLOBAL_POSITION_INT` → Lattice entity PUT |
| **Sim** | Synthetic track without a vehicle (`--sim`) for sandbox smoke |
| **Auth** | OAuth client-credentials + Sandboxes Bearer (`--auth-only`) |

**Primary implementation: C++ (WinHTTP)** - same TLS/auth path proven by Polybolos
HOTL against Lattice sandboxes. A thin Python reference remains under `bridge/`.

## Not in scope

- POLYBOLOS Core / ThreatObject / ROE / magazine / HOTL Authority  
- Merging into `anduril/lattice-sdk-*` (archived / Buf-registry path)  
- Lattice → MAVLink **tasking** (Loiter / investigate / etc.). For a two-way
  gRPC + MAVSDK Linux agent in that lane, see
  [AscendEngineering/lattice-mavlink](https://github.com/AscendEngineering/lattice-mavlink)  
- Full MAVLink dialect (only `GLOBAL_POSITION_INT` decode today)

## Prerequisites

- Windows + CMake 3.21+ + MSVC (WinHTTP)  
- Lattice Sandbox credentials (Developer Program)  
- Optional: a MAVLink vehicle/SITL on UDP  

## Build (C++)

```powershell
cd cpp
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Binary: `cpp\build\Release\mavlink_lattice_bridge.exe`

## Credentials

Same model as Polybolos HOTL sandbox evidence (`docs/LATTICE_SANDBOX_EVIDENCE.md`):

| Variable | Meaning |
|----------|---------|
| `LATTICE_ENDPOINT` | host:port, e.g. `lattice-XXXX.env.sandboxes.developer.anduril.com:443` |
| `LATTICE_CLIENT_ID` | OAuth client id |
| `LATTICE_CLIENT_SECRET` | OAuth client secret |
| `LATTICE_ENV_TOKEN` | Sandboxes Bearer (`Anduril-Sandbox-Authorization`) |
| `ENTITY_ID` | optional; default `polybolos-mavlink-ownship` |

```powershell
copy .env.example .env
# edit .env, then:
#   Get-Content .env | ForEach-Object { if ($_ -match '^([^#=]+)=(.*)$') { Set-Item env:$($matches[1]) $matches[2].Trim('"') } }
```

## Run (C++)

```powershell
# OAuth only
.\cpp\build\Release\mavlink_lattice_bridge.exe --auth-only

# Sandbox smoke - synthetic ownship (no MAVLink hardware)
.\cpp\build\Release\mavlink_lattice_bridge.exe --sim --once

# Live MAVLink UDP (default 14550) → Lattice publish
.\cpp\build\Release\mavlink_lattice_bridge.exe --mavlink-udp 14550
```

## Python reference (optional)

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python -m bridge --sim --once
```

If Python OAuth hits TLS connection reset on Windows, use the C++ binary
(or HOTL `tools/lattice_login.ps1`) - WinHTTP is the proven sandbox client path.

## Architecture

```
MAVLink UDP ──► MavlinkUdpListener ──► VehiclePose
                                         │
                                         ▼
                                 LatticeRestClient ──► PUT /api/v1/entities
```

REST + OAuth client-credentials (sandbox). Keep this door thin so it can sit
beside any C2 that already speaks Lattice or MAVLink.

## Related work

**Peers (different shape):**
- [AscendEngineering/lattice-mavlink](https://github.com/AscendEngineering/lattice-mavlink) - Linux gRPC + MAVSDK bridge with Lattice task execution (telemetry up + commands down)
- [ARK-Electronics/mavlink-to-lattice](https://github.com/ARK-Electronics/mavlink-to-lattice) - Python/SDK MAVLink sample
- [anduril/sample-app-ais-integration-rest](https://github.com/anduril/sample-app-ais-integration-rest) - official AIS sample

**Polybolos doors (this family):**
- [Polybolos-Institute/anduril-opensky-lattice-bridge](https://github.com/Polybolos-Institute/anduril-opensky-lattice-bridge) - ADS-B / OpenSky door
- [Polybolos-Institute/anduril-dump1090-lattice-bridge](https://github.com/Polybolos-Institute/anduril-dump1090-lattice-bridge) - dump1090/readsb door
- [Polybolos-Institute/anduril-lattice-rest-winhttp](https://github.com/Polybolos-Institute/anduril-lattice-rest-winhttp) - shared WinHTTP REST client
- [Polybolos-Institute/anduril-lattice-stream-watcher](https://github.com/Polybolos-Institute/anduril-lattice-stream-watcher) - read-only SSE watcher
- [Polybolos-Institute/anduril-mock-lattice](https://github.com/Polybolos-Institute/anduril-mock-lattice) - local CI Lattice stand-in
- [Polybolos-Institute/anduril-lattice-sandbox-dx](https://github.com/Polybolos-Institute/anduril-lattice-sandbox-dx) - auth checklist + ontology cheat sheet

This repo stays a **Windows REST Sandboxes door** (telemetry / sim → entity PUT). Ascend covers on-vehicle tasking. Complementary lanes.
## License

MIT - see [LICENSE](LICENSE).

Anduril®, Lattice®, and Lattice SDK® are trademarks of Anduril Industries.
This project is an independent integration sample, not an Anduril product.

## Contact

This repository is the open foundation (MIT).

Polybolos Institute also maintains a proprietary catalog of additional capabilities that are not published here. Contact us to discuss production deployment and commercial licensing.

mark.brown@polybolos.org · https://www.polybolos.org
