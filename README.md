# Durst Laborator 1200 — Wireless Display and Control

Retrofitted display and control for the Durst Laborator 1200 enlarger.

Controls:

- Motor up/down (normal or fast)
- Lens up/down (normal or fast)
- Lamp on/off or by timer

Control via:

- TM1638 panel (8 buttons)
- Bluetooth gamepad

Displays:

- TM1638 7‑segment
- Grove LCD (16×2 characters)

Either display can be wired to the master module or used wirelessly via a slave node over ESP‑NOW. The device also starts its own Wi‑Fi access point for setup, so you don’t need to join home Wi‑Fi to use the wireless display/panel.

## Overview

- Master node drives motors, lamp SSR, UI, and broadcasts display over ESP‑NOW.
- Slave node (optional) only renders the broadcasted display on TM1638/LCD.
- Brightness values: 0,1,2,7,OFF(255), persisted to NVS.

## Hardware/Pins (ESP32)

- TM1638: `STB=13`, `CLK=32`, `DIO=14` (high‑freq mode enabled).
- Buzzer: `GPIO16` (LEDC ch 4). Indicator LED disabled.
- Lamp SSR: `GPIO33`.
- Motor 1 (DRV8874): `IN1=25`, `IN2=26`, `CS=35`, `FAULT=27`, `SLEEP` tied HIGH, LEDC ch `0/1`.
- Motor 2 (DRV8874): `IN1=18`, `IN2=19`, `CS=34`, `FAULT=22`, `SLEEP` tied HIGH, LEDC ch `2/3`.

## Wiring

- Diagram: [KiCad/DurstControlRetrofitWiring.pdf](KiCad/DurstControlRetrofitWiring.pdf) or [KiCad/](KiCad/) (draft / work in progress) )
- For connecting the motors, the limit switches and lamp to the right cables see [Durst Service manuals](http://durst.loremi.com/).
- Slave node: same wiring concept; only one or both displays (TM1638 and/or Grove LCD) need to be connected.

## Controls

TM1638 panel (controls only if TM1638 is connected to master):

- `S1` Fast speed (motors or timer adjust)
- `S1+S8` toggle lamp (on/off)
- `S2` timer −0.1s; `S3` timer +0.1s
- `S1+S2` timer -1s; `S1+S3` timer +1s
- `S4` M1 down; `S5` M1 up
- `S6` M2 down; `S7` M2 up
- `S8` start / cancel timer
- Conflicts (e.g., up+down) coast + short beep.

Bluetooth gamepad (Bluepad32):

- `Cross(×)` / `Triangle(△)`: M1 (Head) up/down
- `Square(□)`  /  `Circle(○)` : M2 (Lens) up/down
- Fast motor Speed: `L2`/`R2`,  Insane `L1`
- Timer adjust +/-0.1s: D‑pad Up/Down
- Timer +/-1s: `L2`/`R2` + Dpad Up/Down
- Timer +/-10s: `L1` + Dpad Up/Down
- Start / cancel lamp with timer:  D-Pad Right
- Toggle lamp: D-Pad Left
- Adjust TM1638 Display Brightness: `R1`

## Display & Feedback

- TM1638 text shows the active motor duty or `ERRn`, plus lamp and timer (e.g., `123L  4.5`).
- LCD line 1/2 show a short status and timer; LEDs mirror the buttons mask.
- Display state is broadcast over ESP‑NOW; slaves render the same UI.

## Motor Driver (DRV8874)

- FreeRTOS tasks: duty control (500 Hz) and current sense averaging (up to 100 Hz).
- Active brake: both IN high for a short window, then coast.
- Min duty per direction allows asymmetric thresholds; ADC uses 11 dB attenuation.

## Buzzer

- LEDC‑based tone with one‑shot `esp_timer` auto‑stop. Used on conflict/fault.

## Wi‑Fi, ESP‑NOW & Web UI

- Starts SoftAP and attempts STA using saved creds.
- Endpoints: `GET /wifi/api/status`, `POST /wifi/save`, `POST /wifi/reset`.
- UI: `/index.html` (quick), `/wifi/index.html` (detailed+config).
- ESP‑NOW mesh is initialized; broadcast peer is pre‑added. Display messages use `MsgV1`.

## Build & Flash (PlatformIO)

- Master (default): `pio run -e esp32dev` → upload with `-t upload`.
- Slave (display‑only): `pio run -e esp32slave` → upload with `-t upload`.
- LittleFS: `pio run -t buildfs` and `pio run -t uploadfs`.

Notes: see [platformio.ini](platformio.ini) for board, ports, Bluepad32 framework package, and the pre‑build gzip step ([tools/gzip_fs.py](tools/gzip_fs.py)).

## Repo Layout

- [src/](src/) — [mainMaster.cpp](src/mainMaster.cpp) (master), [mainSlave.cpp](src/mainSlave.cpp) (slave).
- [lib/DRV8874/](lib/DRV8874/) — motor driver and control tasks.
- [lib/Controls/](lib/Controls/), [lib/GamePad/](lib/GamePad/) — input merge and Bluepad32 wrapper.
- [lib/DisplayMux/](lib/DisplayMux/), [lib/TM1638plusWrapper/](lib/TM1638plusWrapper/) — display + broadcast.
- [lib/WifiPortal/](lib/WifiPortal/) — SoftAP/STA, routes, ESP‑NOW init.
- [data_src/](data_src/) → [data/](data/) — web UI, gzipped at build.

## Mesh Password

- default password used for the mesh SoftAP is `changeme`
- Create `include/secrets.h` in [include/](include/) and add `#define MESH_PASS yourpassword` to customize.
- `include/secrets.h` is already in [.gitignore](.gitignore)
