
![Tiny Protocol](.travis/tinylogo.svg)<br>
![Github actions](https://github.com/lexus2k/tinyproto/actions/workflows/main.yml/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/lexus2k/tinyproto/badge.svg?branch=master)](https://coveralls.io/github/lexus2k/tinyproto?branch=master)
[![Documentation](https://codedocs.xyz/lexus2k/tinyproto.svg)](https://codedocs.xyz/lexus2k/tinyproto/)
![License](https://img.shields.io/badge/license-GPLv3-blue)
![License](https://img.shields.io/badge/license-Commercial-blue)

---

**TinyProto** is a lightweight, portable HDLC-based data link layer protocol for embedded systems and microcontrollers. Communicate reliably over UART, SPI, I2C or any byte stream — with automatic retransmission, CRC error detection, and zero dynamic memory allocation.

---

## Table of Contents

- [Introduction](#introduction)
- [Protocol Modes](#protocol-modes)
- [Architecture](#architecture)
- [Key Features](#key-features)
- [Supported Platforms](#supported-platforms)
- [Quick Start](#quick-start)
  - [C API](#c-api)
  - [C++ API](#c-api-1)
  - [Python](#python)
- [Building](#building)
- [Testing](#testing)
- [Installation](#installation)
- [Tools](#tools)
- [API Reference](#api-reference)
- [License](#license)

## Introduction

TinyProto is a **Layer 2 (Data Link)** protocol implementation based on [RFC 1662](https://tools.ietf.org/html/rfc1662) and the [HDLC](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control) specification. It provides reliable, framed communication between two or more devices over any byte-oriented transport (UART, SPI, I2C, USB CDC, TCP sockets, etc.).

The library is designed for resource-constrained environments — from 8-bit AVR microcontrollers with just 60 bytes of SRAM to full Linux/Windows desktop systems. All memory is statically allocated; no `malloc` is ever called.

TinyProto is **not** an application-layer protocol. It can be used as a transport for higher-level protocols such as Protocol Buffers, MessagePack, JSON, or any custom framing you need.

## Protocol Modes

TinyProto supports three protocol layers, from simplest to most capable:

| Layer | API Prefix | Description | Use Case |
|-------|-----------|-------------|----------|
| **Light** | `tiny_light_*` | SLIP-like framing with optional CRC. No acknowledgments. | Simple, low-overhead streaming |
| **HDLC** | `hdlc_*` | Low-level HDLC framing (flag bytes, byte stuffing, CRC). | Custom protocol building blocks |
| **Full-Duplex (FD)** | `tiny_fd_*` | Complete HDLC implementation with sliding window, ACK/NAK, retransmission | Reliable bidirectional communication |

### Full-Duplex Modes (FD)

The FD protocol supports two HDLC operating modes:

#### ABM — Asynchronous Balanced Mode (Peer-to-Peer)

```
┌────────────┐                        ┌────────────┐
│  Device A  │ ◄──── UART/SPI ──────► │  Device B  │
│   (peer)   │      bidirectional     │   (peer)   │
└────────────┘                        └────────────┘
```

- Both devices are **equal peers** — either side can initiate communication
- Connection established via **SABM/UA** exchange
- Supports hot plug/unplug with automatic reconnection
- Ideal for: point-to-point links between two MCUs, MCU ↔ PC communication

#### NRM — Normal Response Mode (Multi-drop / RS-485)

```
┌────────────┐      ┌────────────┐      ┌────────────┐
│  Primary   │─────►│ Secondary1 │      │ Secondary2 │
│  (master)  │◄─────│  addr = 1  │      │  addr = 2  │
│            │─────────────────────────►│            │
│            │◄─────────────────────────│            │
└────────────┘      └────────────┘      └────────────┘
```

- **One primary** station controls communication with **multiple secondaries**
- Primary polls each secondary in round-robin order using P/F (Poll/Final) bit
- Secondaries only transmit when polled
- Connection established via **SNRM/UA** exchange
- Supports full sliding window for each secondary (up to 7 outstanding frames)
- Ideal for: RS-485 buses, multi-drop serial networks, sensor networks

### Frame Types

TinyProto implements all standard HDLC frame types:

| Frame | Type | Description |
|-------|------|-------------|
| **I** | Information | Carries user data with sequence numbers |
| **RR** | Supervisory | Receiver Ready — acknowledges received frames |
| **RNR** | Supervisory | Receiver Not Ready — flow control |
| **REJ** | Supervisory | Reject — requests retransmission from sequence N |
| **SREJ** | Supervisory | Selective Reject — requests retransmission of specific frame |
| **SABM** | Unnumbered | Set ABM — initiates peer-to-peer connection |
| **SNRM** | Unnumbered | Set NRM — initiates primary/secondary connection |
| **DISC** | Unnumbered | Disconnect — terminates connection |
| **UA** | Unnumbered | Unnumbered Acknowledge — confirms mode setting |
| **DM** | Unnumbered | Disconnected Mode — rejects commands when disconnected |
| **FRMR** | Unnumbered | Frame Reject — reports protocol errors |
| **UI** | Unnumbered | Unnumbered Information — connectionless data |
| **RSET** | Unnumbered | Reset — resets sequence counters |

## Architecture

```
┌─────────────────────────────────────────────────┐
│                 Application                      │
├──────────────┬──────────────┬───────────────────┤
│  C++ API     │   C API      │   Python API      │
│  IFd / Hdlc  │  tiny_fd_*   │   tinyproto.*     │
│  Light       │  hdlc_*      │                   │
├──────────────┴──────────────┴───────────────────┤
│           Full-Duplex Protocol (FD)              │
│    ┌─────────────┬────────────┬───────────┐     │
│    │ I-frame     │  S-frame   │  U-frame  │     │
│    │ queue       │  handler   │  handler  │     │
│    └─────────────┴────────────┴───────────┘     │
├─────────────────────────────────────────────────┤
│           HDLC Low-Level Framing                 │
│         (byte stuffing, CRC, flags)              │
├─────────────────────────────────────────────────┤
│           Hardware Abstraction Layer              │
│     (timers, mutexes, serial I/O)                │
├─────────────────────────────────────────────────┤
│     UART  │  SPI  │  I2C  │  USB  │  TCP  │ ... │
└─────────────────────────────────────────────────┘
```

## Key Features

- **Reliable Delivery** — Sliding window (1–7 frames), automatic retransmission, REJ and SREJ recovery
- **Error Detection** — 8-bit checksum, FCS-16 (CCITT), or FCS-32 (CCITT)
- **Two HDLC Modes** — ABM (peer-to-peer) and NRM (primary/secondary multi-drop)
- **Hot Plug/Unplug** — Automatic connection recovery in ABM mode
- **Zero Dynamic Allocation** — All buffers are statically sized; fully deterministic memory usage
- **Tiny Footprint** — From 60 bytes SRAM, 1.3 KiB flash (features are compile-time configurable)
- **Large Frames** — Payload up to 32 KiB (configurable)
- **Full Logging** — Built-in protocol debug logging with CSV export
- **Multi-Language** — C, C++, and Python APIs
- **Portable** — Works on any platform with a C99 compiler

## Supported Platforms

TinyProto runs anywhere a C99/C++11 compiler is available:

| Platform | MCU Examples | Transport |
|----------|-------------|-----------|
| **Arduino** | Uno, Mega, Zero, Due, Nano | UART, SPI |
| **ESP32** | ESP32, ESP32-S2, ESP32-C3 | UART, SPI |
| **ARM Cortex** | STM32, nRF52, Teensy, SAMD | UART, SPI, I2C |
| **AVR** | ATmega328, ATmega2560 | UART |
| **Linux** | Any | UART, TCP, pipes |
| **Windows** | Any | COM ports, TCP |
| **RISC-V** | ESP32-C3, GD32V | UART, SPI |

### Custom Platform Support

If your platform isn't listed, implement the HAL abstraction (timing + mutex functions):

1. Add `TINY_CUSTOM_PLATFORM` to your compiler defines
2. Implement HAL functions and call `tiny_hal_init()`
3. Optionally add `CONFIG_TINYHAL_THREAD_SUPPORT` for multi-threaded environments

See [hal/linux/linux_hal.inl](src/hal/linux/linux_hal.inl) and [hal/esp32/esp32_hal.inl](src/hal/esp32/esp32_hal.inl) for reference implementations, or use the template at [tools/hal_template_functions/platform_hal.c](tools/hal_template_functions/platform_hal.c).

## Quick Start

### C API

**ABM mode (peer-to-peer):**

```c
#include "proto/fd/tiny_fd.h"

// Callback when a complete message is received
void on_frame_received(void *udata, uint8_t addr, uint8_t *buf, int len)
{
    printf("Received %d bytes\n", len);
}

// Allocate protocol buffer (use tiny_fd_buffer_size_by_mtu() to calculate)
uint8_t buffer[tiny_fd_buffer_size_by_mtu(64, 4)];

tiny_fd_handle_t handle;
tiny_fd_init_t init = {
    .pdata        = NULL,
    .on_read_cb   = on_frame_received,
    .buffer       = buffer,
    .buffer_size  = sizeof(buffer),
    .window_frames = 4,
    .send_timeout  = 1000,
    .retry_timeout = 200,
    .retries       = 3,
    .crc_type      = HDLC_CRC_16,
    .mode          = TINY_FD_MODE_ABM,
};

tiny_fd_init(&handle, &init);

// In your main loop:
// 1. Feed received bytes to the protocol
tiny_fd_on_rx_data(handle, rx_bytes, rx_len);

// 2. Get bytes to transmit
uint8_t tx_buf[64];
int tx_len = tiny_fd_get_tx_data(handle, tx_buf, sizeof(tx_buf), 0);
if (tx_len > 0) {
    serial_write(tx_buf, tx_len);
}

// 3. Send application data
tiny_fd_send_packet(handle, "Hello", 6, 1000);
```

**NRM mode (multi-drop primary):**

```c
tiny_fd_init_t init = {
    // ... same as above, plus:
    .mode         = TINY_FD_MODE_NRM,
    .peers_count  = 3,           // support up to 3 secondaries
    .addr         = TINY_FD_PRIMARY_ADDR,  // this is the primary station
};
```

**NRM mode (secondary station):**

```c
tiny_fd_init_t init = {
    // ... same as above, plus:
    .mode         = TINY_FD_MODE_NRM,
    .addr         = 1,           // unique secondary address (1-63)
};
```

### C++ API

**Full-duplex with static buffer (Arduino-friendly):**

```cpp
#include "TinyProtocolFd.h"

// Static buffer — no heap allocation
tinyproto::Fd<1024> proto;

void onReceive(void *udata, uint8_t addr, tinyproto::IPacket &pkt) {
    // Process received message
    Serial.write(pkt.data(), pkt.size());
}

void setup() {
    Serial.begin(115200);
    proto.setReceiveCallback(onReceive);
    proto.setWindowSize(4);
    proto.enableCrc16();
    proto.begin();
}

void loop() {
    // Feed serial data to protocol
    if (Serial.available()) {
        uint8_t byte = Serial.read();
        proto.run_rx(&byte, 1);
    }
    // Transmit protocol data
    uint8_t tx;
    if (proto.run_tx(&tx, 1) == 1) {
        Serial.write(tx);
    }
}
```

**Dynamic buffer (desktop / powerful MCUs):**

```cpp
#include "TinyProtocolFd.h"

// Dynamic allocation — buffer size determined at runtime
tinyproto::FdD proto(4096);
proto.setWindowSize(7);
proto.enableCrc32();
proto.begin();

// Send data
proto.write("Hello World", 12);
```

### Python

```python
import tinyproto

proto = tinyproto.Hdlc()

def on_read(data):
    print("Received:", data.hex())

proto.on_read = on_read
proto.begin()

# Feed raw bytes from your transport
proto.rx(bytearray([0x7E, 0xFF, 0x3F, 0xF3, 0x39, 0x7E]))

# Queue data and get encoded bytes to transmit
proto.put(bytearray(b"Hello"))
tx_data = proto.tx()
```

## Building

### Linux

```bash
# Using Make
make

# Using CMake
mkdir build && cd build
cmake -DEXAMPLES=ON ..
make

# Run unit tests
make ARCH=linux unittest
./bld/unit_test
```

### Windows

```bash
mkdir build && cd build
cmake -G "Visual Studio 16 2019" -DEXAMPLES=ON ..
# Open the generated .sln file in Visual Studio
```

### ESP32 (IDF)

Place the library in your project's `components/` directory, then build normally with `idf.py build` or `make`.

### AVR

```bash
make ARCH=avr
```

## Testing

TinyProto has a comprehensive test suite with **101 tests** covering all protocol layers:

```bash
# Build and run all tests
make ARCH=linux DEBUG_MODE=y unittest && ./bld/unit_test

# Run specific test groups
./bld/unit_test -g TINY_FD_ABM    # ABM mode tests (28 tests)
./bld/unit_test -g TINY_FD_NRM    # NRM mode tests (15 tests)
./bld/unit_test -g CPP_FD         # C++ FD wrapper tests (10 tests)
./bld/unit_test -g CPP_HDLC       # C++ HDLC wrapper tests (3 tests)
./bld/unit_test -g CPP_LIGHT      # C++ Light wrapper tests (3 tests)
./bld/unit_test -g HDLC           # Low-level HDLC tests
./bld/unit_test -g LIGHT          # Light protocol tests
```

## Installation

### Arduino

**Option 1** — From source:
1. Download from https://github.com/lexus2k/tinyproto
2. Copy to `Arduino/libraries/tinyproto`
3. Restart Arduino IDE → Examples → tinyproto

**Option 2** — Library Manager:
1. Arduino IDE → Sketch → Include Library → Manage Libraries
2. Search for "tinyproto" and install

### ESP32 IDF

```bash
cd your_project/components
git clone https://github.com/lexus2k/tinyproto
```

### PlatformIO

Add to `platformio.ini`:
```ini
lib_deps = lexus2k/tinyproto
```

### Python

```bash
cd tinyproto
python setup.py install
```

## Tools

### tiny_loopback

A serial loopback testing tool for benchmarking and debugging:

```bash
# Build
make ARCH=linux

# Test Light protocol
./bld/tiny_loopback -p /dev/ttyUSB0 -t light -g -c 8 -a -r

# Test Full-Duplex protocol
./bld/tiny_loopback -p /dev/ttyUSB0 -t fd -c 8 -w 3 -g -a -r
```

## API Reference

Full API documentation is available at [codedocs.xyz/lexus2k/tinyproto](https://codedocs.xyz/lexus2k/tinyproto/).

### Key C Functions

| Function | Description |
|----------|-------------|
| `tiny_fd_init()` | Initialize FD protocol instance |
| `tiny_fd_close()` | Close and free FD instance |
| `tiny_fd_on_rx_data()` | Feed received bytes to protocol |
| `tiny_fd_get_tx_data()` | Get bytes to transmit |
| `tiny_fd_send_packet()` | Queue data for transmission |
| `tiny_fd_send_to()` | Send to specific peer address (NRM) |
| `tiny_fd_get_status()` | Check connection status |
| `tiny_fd_disconnect()` | Initiate disconnect |

### Key C++ Classes

| Class | Description |
|-------|-------------|
| `tinyproto::Fd<N>` | FD protocol with static N-byte buffer |
| `tinyproto::FdD` | FD protocol with dynamic buffer allocation |
| `tinyproto::Hdlc` | HDLC low-level framing |
| `tinyproto::Light` | Lightweight SLIP-like protocol |
| `tinyproto::IPacket` | Packet buffer for data exchange |

## License

Dual licensed under **GPLv3** and **Commercial** licenses.

Copyright 2016-2025 (C) Alexey Dynda

**GNU General Public License:** Protocol Library is free software under the GNU Lesser General Public License v3.0 or later. See [COPYING](COPYING) for details.

**Commercial License:** Available for proprietary use. Contact via email on the [GitHub account](https://github.com/lexus2k) for details.

---

<p align="center">
  <sub>If you find this library useful, consider supporting its development.</sub>
</p>

| Bitcoin | Ethereum |
|:-------:|:--------:|
| ![BTC](.travis/btc_segwit.png) | ![ETH](.travis/eth.png) |
| `3CtUY6Ag2zsvm1JyqeeKeK8kjdG7Tnjr5W` | `0x20608A71470Bc84a3232621819f578Fb9C02A460` |
