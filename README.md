# iot-smart-distribution-board
# IoT-Based Electrical Load Monitoring and Peak Load Shedding in a Distribution Board

A smart distribution board that monitors each sub-circuit and device and disconnects **only the circuit that overloads**, instead of tripping the whole house. B.Tech project, Dept. of EEE, Mar Baselios College of Engineering and Technology, Thiruvananthapuram.

## Features
- Measures voltage, current, power, energy, frequency and power factor per circuit and device (PZEM-004T V3.0)
- Per-circuit limits within the approved load; the server rejects limits that exceed it
- Automatic shedding on the ESP32, with reconnection after 30 s and lockout after repeated overloads
- Limits are stored in flash, so protection keeps working without Wi-Fi
- Flask + SQLite server with a web dashboard, event log and CSV export
- SMS or call alerts using a SIM800L GSM module
- Aerosol fire-extinguishing unit inside the enclosure, with MCBs still providing standard protection

## Hardware
ESP32, 3 x PZEM-004T V3.0, 3 relay modules, SIM800L GSM module, 25 A main MCB, 6/16/25 A sub-circuit MCBs, aerosol fire-extinguishing unit, distribution board enclosure.

## How it works
The ESP32 reads the PZEM modules every second and compares each circuit with its limit. If a circuit stays above its limit, only its relay opens. It sends readings to the Flask server every 5 seconds and receives updated limits in return.

## Run it
**Server**
```bash
pip install -r requirements.txt
python app.py
```
**Firmware:** open the `.ino` file in Arduino IDE, enter your Wi-Fi details and server IP, and upload to the ESP32.

## Safety
This is a prototype. The relays supplement the MCBs and do not replace them. Mains wiring should be done by a qualified person.

## Status
Shedding logic verified with firmware tests and simulation. Real-load testing is in progress.

## Authors
Allen Anil, Alin Stanly, Irfana M, Jonath Vinamsi
Guides: Ms. Surasmi N L, Ms. Sheenu P
