# Automatic Window Opener (AWO)

Firmware for an automated classroom window control system, 
developed as an electronics engineering project at NZSE. 
The system senses indoor and outdoor conditions and automatically controls window position via a linear actuator, with manual override, fault detection, data logging, and a Wi-Fi dashboard.

## System Overview

The AWO consists of two ESP32-based modules:

- **Indoor module (ESP32-S3)** — runs the main control logic, drives the linear actuator via a BTS7960 motor driver, manages the LCD, rotary encoder, SD card logging, and hosts the Wi-Fi web dashboard.
- **Outdoor module (ESP32-C6)** — reads outdoor temperature, rain, and wind sensors, and transmits readings to the indoor module via ESP-NOW.

## Firmware Structure

| File | Responsibility |
|---|---|
| `main.cpp` | Initialises all components and runs the main loop. |
| `config.h` | Pin assignments, thresholds, timing constants, mode/state enums, credentials. |
| `statemachine.h/.cpp` | Manages Automatic, Manual, and Locked modes; enforces the safety priority hierarchy. |
| `motor.h/.cpp` | Actuator commands (open, close, stop) via the BTS7960. |
| `sensors.h/.cpp` | Environmental sensor and button readings. |
| `encoder.h/.cpp` | Rotary encoder input handling. |
| `display.h/.cpp` | LCD1602 row-level display helpers. |
| `buzzer.h/.cpp` | Buzzer control (on, off, timed). |
| `espnow.h/.cpp` | ESP-NOW communication between modules; Wi-Fi connection setup. |
| `webserver.h/.cpp` | Web dashboard frontend/backend, login, and dashboard commands. |
| `storage.h/.cpp` | Wi-Fi/login credentials and settings persistence. |
| `logger.h/.cpp` | SD card data logging. |

## Operating Modes

- **Automatic** — window position is controlled based on indoor/outdoor temperature, rain, and wind conditions.
- **Manual** — user sets window position directly via the rotary encoder or dashboard.
- **Locked** — actuator movement is disabled; used to prevent unintended adjustment.
- **Teacher Override** — a priority mode accessible via the dashboard, overriding the current mode.

## Operating Instructions

### Rotary Encoder Controls

| Action | Function |
|---|---|
| Rotate | Scroll through menu options, modes, or adjust a value (setpoint/position) one step at a time |
| Single press | Select / confirm option |
| Double press | Go back |
| Hold for 3 seconds (during E-STOP or Overcurrent fault state) | Recovery gesture — returns the system to Manual mode (from E-STOP) or the previous mode (from Overcurrent) |

### LCD Display

Shows current mode, window position, live sensor readings, and error codes when a fault is active.

### Web Dashboard

Accessible over the local Wi-Fi network once the indoor module connects. Displays live sensor data, window position, and mode status. Allows remote position/mode control and supports a password-protected Teacher Override login. Logged data is viewable and downloadable as a CSV file directly from the dashboard.

### Control Password

Password for the controls for the Web Dashboard.

Normal: user1234
Teacher: teacher1234

### Safety Behaviour

The system follows a fixed safety priority order, checked continuously:

1. **Emergency Stop** — immediately halts all motor activity.
2. **Overcurrent Protection** — detected via motor current sensing; halts actuator movement.
3. **Natural Events (rain/wind)** — forces the window closed and engages a storm lockout timer.
4. **Mode logic** — Automatic, Manual, or Locked behaviour, only evaluated once the above are clear.

The buzzer sounds when Locked mode is engaged, when Emergency Stop is triggered, when a natural event becomes active, or when an overcurrent condition is detected.

## Known Issues

- **Manual mode setpoint override:** changing the target position while the actuator is still moving toward a previous target can cause it to overshoot to a physical limit switch rather than stopping at the new position. See project report, Section 5.5, for details and proposed fix.
