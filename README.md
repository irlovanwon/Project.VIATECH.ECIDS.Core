# ECIDS Core

Core orchestration module for the Escalator Clearance Inspection Device (ECIDS).

## Overview

Bridges StereoCamera, AIVisionServerDealer, and the GUI to perform escalator tread gap clearance inspection.

## Build

```bash
cd /home/user/ECIDS/Core
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
./build/ecids_core_node [config_dir] [cert_path] [key_path]
```

## Service Management

```bash
./scripts/start.sh start    # Build (if needed) and start
./scripts/start.sh stop     # Stop
./scripts/start.sh restart  # Restart
./scripts/start.sh status   # Check status
./scripts/start.sh build    # Build only
```

## Dependencies

- nlohmann_json, ZeroMQ (libzmq), libcurl, OpenSSL, Google Test
