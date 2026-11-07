# iot-smart-distribution-board

A smart distribution board that monitors each device and disconnects **only the device that over consumes energy**, instead of tripping the whole house.

## Features
- Measures voltage, current, power, energy, frequency and power factor per circuit and device (PZEM-004T V3.0)
- Per-device energy limits within the approved load; the server rejects limits that exceed it
- Automatic shedding on the ESP32, with reconnection after 30 s 
- Limits are stored in flash, so protection keeps working without Wi-Fi
- Flask + SQLite server with a web dashboard, event log and CSV export
- A buzzer along with SMS or call alerts using a SIM800L GSM module
- Aerosol fire-extinguishing unit inside the enclosure, with MCBs still providing standard protection

## Hardware
ESP32, 3 x PZEM-004T V3.0, 3 relay modules, SIM800L GSM module, 25 A main MCB, 6/16/25 A sub-circuit MCBs, aerosol fire-extinguishing unit, distribution board enclosure.

## How it works
The ESP32 reads the PZEM modules every second and compares each device with its energy consumption limit. If a device stays above its limit, a call and SMS is sent to the user along with a notification to the iot dashboard for the user to confirm the shedding of the load, only then the relay opens. It sends readings to the Flask server every 5 seconds and receives updated values in return. A standalone aerosol fire-extinguishing unit is also added to the system to provide additional fire protection.

## Safety
This is a prototype. The relays supplement the MCBs and do not replace them. Mains wiring should be done by a qualified person.
