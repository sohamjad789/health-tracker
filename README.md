# Screenless Wearable Health Tracker — Wokwi Simulation

This project validates the tracker data path before hardware is purchased:

`virtual MPU6050 → ENMO motion intensity → synthetic heart rate → calorie fusion → Heart Rate Service payload`

## Important simulator limitation

Wokwi does **not** officially support nRF52840 at the time this project was created. `diagram.json` therefore uses an ESP32 only as a simulator stand-in for I2C, motion input, and serial logs. The `sketch.ino` file still contains the standard Adafruit Bluefruit nRF52 Heart Rate Service implementation, protected by `ARDUINO_ARCH_NRF52`, for a real nRF52 build.

This means Wokwi validates the IMU, synthetic heart-rate, calorie, and Heart Rate Measurement **payload** logic. It does not emulate BLE radio advertising or let a phone connect. The Wokwi fallback prints `SIM BLE HRS notify` lines instead.

## Files

- `diagram.json` — ESP32 simulator stand-in connected to `wokwi-mpu6050` over I2C: GPIO 21 SDA and GPIO 22 SCL.
- `sketch.ino` — single firmware source. It reads motion every loop, produces synthetic HR, calculates calories, and contains guarded Bluefruit HRS code.
- `src/main.cpp` — minimal PlatformIO entry point that includes `sketch.ino`; do not add application logic here.
- `libraries.txt` — Wokwi web-editor dependencies.
- `platformio.ini` — PlatformIO build configuration for the Wokwi ESP32 simulator target.
- `wokwi.toml` — local Wokwi/VS Code firmware configuration and serial-forwarding port.
- `automation.yaml` — the 60-second motion scenario.

## Run in Wokwi web editor

1. Create an ESP32 project at Wokwi. This is the temporary simulator target, not the final tracker MCU.
2. Replace the generated `diagram.json` and `sketch.ino` with the matching files from this project.
3. In Wokwi's Library Manager, add `Adafruit MPU6050` and `Adafruit Unified Sensor`, or add the contents of `libraries.txt` as the project library list.
4. Start the simulation and open the Serial Monitor. It will print HR, ENMO, calorie rate, total calories, and `SIM BLE HRS notify` events.

## Run from VS Code / PlatformIO

1. Open this folder in VS Code with the PlatformIO and Wokwi extensions installed.
2. Build with PlatformIO's checkmark button. The build downloads the ESP32 platform and the two libraries defined in `platformio.ini`.
3. Open `diagram.json`, then run **Wokwi: Start Simulator**.
4. If using Wokwi's TCP serial forwarding, set the Serial Monitor to TCP host `localhost`, port `4000`.

## Run the automation scenario

Wokwi automation scenarios are currently a CLI/CI feature. After building and configuring a Wokwi CLI token, run:

```bash
wokwi-cli . --scenario automation.yaml --timeout 65000
```

The three phases are deliberately 20 seconds each:

- **Resting/still** — `X=0`, `Y=0`, `Z=1g`; ENMO should remain near zero.
- **Walking** — low-amplitude X/Y/Z oscillation; ENMO and the motion fallback increase.
- **Vigorous/running** — stronger, faster oscillation; ENMO becomes noticeably larger.

## What changes on real hardware

- Replace the two `FAKE DATA GENERATOR` blocks in `sketch.ino` with SparkFun MAX30101/MAX32664 initialization and validated BPM reads.
- Replace `Wire.begin(21, 22)` with the XIAO nRF52840 Sense I2C configuration, then replace the external MPU6050 driver with the driver for the XIAO Sense onboard IMU.
- Build for the physical XIAO with a board package and BLE library that are actually compatible with it. The Bluefruit section follows Adafruit nRF52 conventions; confirm XIAO/Bluefruit compatibility or port the standard HRS code to the BLE stack you select.
- Remove the ESP32/Wokwi fallback branch after the physical BLE stack is validated; the `BLEService(0x180D)`, measurement payload `{0x00, bpm}`, advertising flow, and notification cadence are the intended BLE behavior.
- Calibrate or replace the calorie model with a formula validated for your intended population and product use. The included Keytel/MET fusion is a prototype, not medical guidance.
