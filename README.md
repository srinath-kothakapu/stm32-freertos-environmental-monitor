# FreeRTOS Environmental Monitor on STM32 Blue Pill

This project is a small, multi-task environmental-monitoring firmware demo for the STM32F103C8T6 "Blue Pill." It reads a DHT11 temperature/humidity sensor, checks configurable thresholds, updates an alert LED and buzzer, displays the latest state on a 16x2 I2C LCD, and streams a CSV record over UART.

The firmware uses the STM32duino Arduino framework with the STM32duino FreeRTOS library. Sensor sampling, alert decisions, display updates, and serial logging are separated into tasks to demonstrate a simple RTOS design with a queue, shared-state synchronization, and task notifications. A companion Python utility captures the UART stream and summarizes the firmware-reported alert latency.

> **Hardware validation status:** this is a new implementation of the project idea and has been syntax-checked, but it has not yet been flashed and verified on a physical board. No measured latency or sensor results are claimed in this repository. Please record your own board and bench results after testing.

## What the project demonstrates

- Dividing embedded work into four FreeRTOS tasks with different priorities.
- Passing sensor readings to the alert logic through a queue.
- Updating shared display/log state under synchronization and notifying consumers when a new state is ready.
- Sampling a DHT11 every two seconds, which respects its minimum read interval.
- Driving local warning outputs when the configured temperature or humidity limit is exceeded.
- Keeping the user-visible display and UART log in step with the alert decision.
- Capturing a serial stream to CSV and calculating sample and latency summaries with a desktop tool.

## System architecture

```text
DHT11
  │ sample every 2 seconds
  ▼
SensorTask (priority 2)
  │ queued reading + tick time
  ▼
AlertTask (priority 3) ──> LED + buzzer
  │ shared state / notifications
  ├────────────────────────────> DisplayTask (priority 1) ──> I2C LCD
  └────────────────────────────> LogTask (priority 1) ─────> UART CSV
                                                                  │
                                                                  ▼
                                                tools/capture_uart.py
```

| Task | Priority | When it runs | Responsibility |
|---|---:|---|---|
| `SensorTask` | 2 | Every 2,000 ms | Reads temperature and humidity; puts the reading and tick timestamp onto the queue |
| `AlertTask` | 3 | For each queued reading | Checks thresholds, updates the LED/buzzer, records the alert state, and measures the configured read-to-output interval |
| `DisplayTask` | 1 | On notification | Shows temperature, humidity, and the current `OK` or alert state on the LCD |
| `LogTask` | 1 | On notification | Writes a timestamped CSV-compatible row to the serial port |

The alert task has higher priority than the display and logging tasks so the local warning outputs are handled before those slower presentation tasks. This is the intended design; its actual timing and scheduling behavior still need to be measured on target hardware.

## Hardware and wiring

The pin assignments below are the defaults in [`src/config.h`](src/config.h). Confirm every connection against your board revision and the specific modules you own before applying power.

| Component signal | Blue Pill pin | Notes |
|---|---|---|
| DHT11 data | PB12 | Use a 3.3 V data level and a 10 kOhm pull-up to 3.3 V if the module does not include one |
| Alert LED | PB0 | Connect through a current-limiting resistor |
| Active buzzer | PB1 | Check the buzzer's current requirement; use a driver transistor if needed |
| LCD I2C SCL | PB6 | I2C1 clock |
| LCD I2C SDA | PB7 | I2C1 data |
| UART TX to USB-UART RX | PA9 | USART1, 115200 baud, 3.3 V logic |
| UART RX from USB-UART TX | PA10 | Cross TX/RX and share ground; firmware logging uses TX |
| Ground | GND | Join the board, sensor, LCD, output-driver, and USB-UART grounds |

The firmware assumes a 16-column by 2-row LCD with a PCF8574 I2C backpack at address `0x27`. Some backpacks use `0x3F`; change `LCD_I2C_ADDR` in `src/config.h` if required. DHT11 data is sampled every two seconds. Do not power 3.3 V-only inputs from a 5 V signal.

### Parts used by this example

- STM32F103C8T6 Blue Pill and ST-Link V2 programmer
- DHT11 temperature/humidity module
- 16x2 character LCD with I2C backpack
- External LED with resistor and an active buzzer
- 3.3 V logic USB-to-UART adapter for serial capture
- Jumper wires and an appropriate regulated power supply

## Build and flash

### Requirements

- [PlatformIO Core](https://platformio.org/install/cli) or the PlatformIO extension for VS Code
- ST-Link V2 and target-board wiring for upload
- A serial terminal or USB-to-UART adapter for viewing logs

The board, framework, upload protocol, baud rate, and library dependencies are declared in [`platformio.ini`](platformio.ini). PlatformIO installs the declared libraries when building.

```bash
git clone https://github.com/srinath-kothakapu/stm32-freertos-environmental-monitor.git
cd stm32-freertos-environmental-monitor
pio run                  # compile for the configured Blue Pill
pio run -t upload        # program over ST-Link
pio device monitor       # open the UART monitor at 115200 baud
```

A successful compile only verifies that the source builds for the selected toolchain; it does not demonstrate that the firmware works on a physical board. Check that the selected ST-Link and board are connected before uploading. If you only want to inspect or test the Python UART-capture helper, that can be done without flashing the firmware.

## Alert behavior

The default limits are defined in `src/config.h`:

| Reading | Alert condition | Default limit |
|---|---|---:|
| Temperature | Above the temperature limit | 30 C |
| Relative humidity | Above the humidity limit | 70% |

When either condition is met, the alert task updates the local LED/buzzer state and notifies the other tasks. These thresholds are demonstration values; they are not calibrated health, safety, or industrial limits. The LCD and alert behavior must be confirmed on the hardware before use.

## UART output and latency capture

At 115200 baud, the firmware emits a header and one comma-separated row per reading:

```text
tick_ms,temperature_c,humidity_pct,alert,latency_ms
```

`tick_ms` is the firmware tick timestamp, `alert` indicates the alert state, and `latency_ms` is the firmware's elapsed time from the sensor read to updating the alert output. This is an implementation-defined measurement, not an independently validated end-to-end response-time guarantee.

The Python capture helper needs Python 3 and the packages in [`tools/requirements.txt`](tools/requirements.txt). It can capture directly from a serial port or summarize a previously saved capture:

```bash
python -m pip install -r tools/requirements.txt

# Capture 120 seconds from a Windows serial port
python tools/capture_uart.py --port COM5 --seconds 120 --out captures/run1.csv

# Or use a port such as /dev/ttyUSB0 on Linux
python tools/capture_uart.py --port /dev/ttyUSB0 --seconds 120 --out captures/run1.csv

# Summarize an existing capture without connecting to a board
python tools/capture_uart.py --from-file captures/run1.csv
```

The summary reports sample count, alert count, and minimum, mean, and maximum of the logged `latency_ms` values. The helper also has parser tests:

```bash
python -m pytest tools
```

A valid row contains exactly five comma-separated values, with integer tick, alert, and latency fields and numeric temperature and humidity fields. Invalid or partial lines are skipped, which is useful when a serial connection opens in the middle of a transmission.

## Project layout

```text
platformio.ini          PlatformIO target, framework, libraries, upload and monitor settings
src/main.cpp            FreeRTOS tasks and application behavior
src/config.h            Board pin map, LCD settings, thresholds, and sampling period
tools/capture_uart.py   UART capture, CSV parsing, and latency summary
 tools/requirements.txt Python capture-tool dependencies
 tools/test_capture.py  Tests for parsing and summary calculations
captures/               Destination for local UART captures; generated runs are not committed
```

## Troubleshooting

- **LCD is blank:** confirm common ground and I2C wiring, adjust the LCD contrast, and try address `0x3F` in `src/config.h` if the backpack does not respond at `0x27`.
- **DHT11 returns an error:** verify its data pin and pull-up, use the correct supply and ground, and keep the sample interval at least one second.
- **No UART output:** confirm the USB-UART adapter uses 3.3 V logic, cross PA9 TX to adapter RX, connect ground, and select 115200 baud.
- **ST-Link upload fails:** check the SWD wiring and BOOT0 state for your board; ensure PlatformIO is targeting the correct port/programmer.
- **Python cannot open the serial port:** close any other terminal using it and substitute the actual port name shown by your operating system.

## Results to add after hardware testing

No bench measurements are included yet. After testing, document the actual configuration rather than estimating results:

- Blue Pill board variant, programmer, power source, DHT11 module, and LCD backpack address.
- A wiring/setup photo and a photo showing normal and alert states.
- Captured sample count and observed sensor values.
- The capture tool's minimum, mean, and maximum reported latency.
- A description of how the LED, buzzer, LCD, and UART behaved at both threshold crossings and recovery.
- Any known reset, sensor-read, timing, or upload issues.

## Limitations and safe use

- Firmware has not yet been flashed and validated on physical hardware; results are therefore intentionally marked unmeasured.
- DHT11 readings and the threshold values are not calibrated for safety or process control.
- The latency value is reported by the firmware and needs independent hardware validation before it can support timing claims.
- Electrical levels, current limits, pull-ups, and module pinouts vary; verify hardware documentation before wiring or powering the circuit.

This is an educational embedded-systems project, not a certified environmental alarm or safety device.

## License

This project is distributed under the MIT License. See [`LICENSE`](LICENSE) if present in your local project checkout.