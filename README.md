# io-sched-framework

A pluggable I/O scheduling framework that multiplexes Arduino hardware access for multiple applications on Raspberry Pi, with runtime-swappable scheduling policies (FIFO, Priority, SJF, SRTF, EDF, Round Robin).

## Project Structure

```
io-sched-framework/
├── CMakeLists.txt                     # Top-level build configuration
│
├── common/                            # Shared definitions (daemon ↔ client)
│   └── include/
│       └── iosched/
│           ├── protocol.h             # Message format, command codes
│           └── types.h                # Common types (device_id, priority, etc.)
│
├── daemon/                            # I/O scheduling daemon
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── daemon/
│   │       ├── scheduler.h            # Scheduler interface (vtable)
│   │       ├── request_queue.h        # Request queue data structure
│   │       ├── serial.h               # UART serial communication wrapper
│   │       ├── client_handler.h       # Unix socket client management
│   │       └── device_registry.h      # Device registration & duration estimation
│   └── src/
│       ├── main.c                     # Daemon entry point, signal handling
│       ├── scheduler.c                # Scheduler dispatch, runtime policy swap
│       ├── request_queue.c
│       ├── serial.c
│       ├── client_handler.c
│       ├── device_registry.c
│       └── policy/                    # Scheduling policy implementations
│           ├── fifo.c
│           ├── priority.c
│           ├── sjf.c
│           ├── srtf.c
│           ├── edf.c
│           └── round_robin.c
│
├── libiosched/                        # Client library
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── iosched.h                  # Public API (io_connect, io_read, io_write, ...)
│   └── src/
│       └── iosched.c                  # Unix socket communication, request/response serialization
│
├── firmware/                          # Arduino firmware
│   ├── firmware.ino                   # Main loop, serial command parsing
│   ├── devices.h                      # Sensor/actuator pin mapping
│   └── devices.c                      # Per-device read/write implementation
│
├── examples/                          # Sample applications
│   ├── CMakeLists.txt
│   ├── temp_monitor.c                 # Periodic temperature sensor reading
│   ├── led_controller.c              # LED control
│   └── motor_driver.c                # Motor control
│
├── bench/                             # Benchmarks
│   ├── CMakeLists.txt
│   └── sched_bench.c                  # Per-policy response time & throughput measurement
│
└── docs/
    └── protocol.md                    # Communication protocol specification
```
