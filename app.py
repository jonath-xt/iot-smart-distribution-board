from flask import Flask, render_template, request, jsonify
from datetime import datetime
import time
import json
from pathlib import Path

app = Flask(__name__)

DEVICE_NAMES = {"room1": "Iron Box", "room2": "Kettle", "room3": "Hair Dryer"}


ESP32_TIMEOUT_SECONDS = 15
SETTINGS_FILE = Path(__file__).with_name("energy_settings.json")
DEFAULT_SETTINGS = {"tariff": 8.0, "limits": {r: {"limit": 10.0} for r in DEVICE_NAMES}}

def load_settings():
    try:
        saved = json.loads(SETTINGS_FILE.read_text(encoding="utf-8"))
        tariff = float(saved["tariff"])
        if not (0 <= tariff <= 1000):
            raise ValueError("Invalid tariff")
        limits = {}
        for room in DEVICE_NAMES:
            item = saved["limits"][room]
            limit = float(item.get("limit", item.get("first")))
            if not (0.1 <= limit <= 100):
                raise ValueError("Invalid energy limit")
            limits[room] = {"limit": limit}
        return {"tariff": tariff, "limits": limits}
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        return json.loads(json.dumps(DEFAULT_SETTINGS))

settings = load_settings()

@app.route("/api/settings", methods=["GET", "POST"])
def api_settings():
    global settings
    if request.method == "GET":
        return jsonify(settings)
    payload = request.get_json(silent=True) or {}
    try:
        tariff = float(payload["tariff"])
        if not (0 <= tariff <= 1000):
            raise ValueError("Tariff must be between 0 and 1000")
        limits = {}
        for room in DEVICE_NAMES:
            limit = float(payload["limits"][room]["limit"])
            if not (0.1 <= limit <= 100):
                raise ValueError(f"Limit for {DEVICE_NAMES[room]} must be 0.1–100 kWh")
            limits[room] = {"limit": limit}
    except (KeyError, ValueError, TypeError) as exc:
        return jsonify({"success": False, "error": str(exc)}), 400
    new_settings = {"tariff": tariff, "limits": limits}
    try:
        SETTINGS_FILE.write_text(json.dumps(new_settings, indent=2), encoding="utf-8")
    except OSError as exc:
        return jsonify({"success": False, "error": str(exc)}), 500
    for room in DEVICE_NAMES:
        if settings["limits"][room]["limit"] != limits[room]["limit"]:
            energy_data[room]["alert_stage"] = 0
            energy_data[room]["pending"] = 0
    settings = new_settings
    return jsonify({"success": True, **settings})

last_esp32_seen = None  # monotonic clock; resets when Flask restarts


# =====================================================
# GLOBAL ENERGY DATA
# =====================================================

energy_data = {

    "room1": {
        "voltage": 0,
        "current": 0,
        "power": 0,
        "energy": 0,
        "frequency": 0,
        "pf": 0,
        "relay": True,
        "pending": False
    },

    "room2": {
        "voltage": 0,
        "current": 0,
        "power": 0,
        "energy": 0,
        "frequency": 0,
        "pf": 0,
        "relay": True,
        "pending": False
    },

    "room3": {
        "voltage": 0,
        "current": 0,
        "power": 0,
        "energy": 0,
        "frequency": 0,
        "pf": 0,
        "relay": True,
        "pending": False
    },


    "last_update": "Never"
}


# =====================================================
# PENDING TRIP COMMAND
# =====================================================

pending_command = None


# =====================================================
# NOTIFICATION
# =====================================================
trip_commands = {
    "room1": "NONE",
    "room2": "NONE",
    "room3": "NONE"
}

notification = {
    "active": False,
    "room": 0,
    "power": 0,
    "energy": 0,
    "power_limit": 0,
    "energy_limit": 0,
    "reason": "",
    "time": ""
}


# =====================================================
# DASHBOARD
# =====================================================

@app.route("/")
def dashboard():

    return render_template(
        "dashboard.html"
    )


# =====================================================
# ESP32 SENSOR DATA
# =====================================================

@app.route(
    "/api/update",
    methods=["POST"]
)
def update_data():

    global energy_data, last_esp32_seen

    data = request.get_json()

    if not data:

        return jsonify({
            "status": "error",
            "message": "No JSON data"
        }), 400


    for room in [
        "room1",
        "room2",
        "room3"
    ]:

        if room in data:

            previous = energy_data.get(room, {})
            energy_data[room] = {**previous, **data[room]}


    last_esp32_seen = time.monotonic()

    energy_data["last_update"] = \
        datetime.now().strftime(
            "%Y-%m-%d %H:%M:%S"
        )


    print()
    print("======================================")
    print("ESP32 DATA RECEIVED")
    print("======================================")

    print(energy_data)


    return jsonify({
        "status": "success"
    })


# =====================================================
# ESP32 SENDS ALERT
# =====================================================

@app.route("/api/alert", methods=["POST"])
def receive_alert():

    try:

        data = request.get_json()

        room = data.get("room")

        if room not in ["room1", "room2", "room3"]:
            return jsonify({
                "success": False,
                "error": "Invalid room"
            }), 400

        # New threshold stage can create a new alert even after an earlier decision.
        stage = 2 if data.get("reason") == "SECOND_ENERGY_LIMIT_EXCEEDED" else 1
        if stage <= energy_data[room].get("alert_stage", 0):
            return jsonify({"success": True, "ignored": True})
        energy_data[room]["alert_stage"] = stage

        # Create notification
        energy_data[room]["pending"] = 1
        energy_data[room]["reason"] = data.get("reason", "ENERGY_LIMIT_EXCEEDED")
        energy_data[room]["energy_limit"] = data.get("energy_limit", 10.0)

        energy_data[room]["alert_handled"] = False

        energy_data[room]["power"] = data.get(
            "power",
            energy_data[room].get("power", 0)
        )

        energy_data[room]["energy"] = data.get(
            "energy",
            energy_data[room].get("energy", 0)
        )

        print(
            f"NEW ALERT -> {room}"
        )

        return jsonify({
            "success": True
        })

    except Exception as e:

        print("Alert error:", e)

        return jsonify({
            "success": False,
            "error": str(e)
        }), 500
# =====================================================
# DASHBOARD DATA
# =====================================================

@app.route("/api/data")
def get_data():

    response = {k: (v.copy() if isinstance(v, dict) else v) for k, v in energy_data.items()}

    response["notification"] = notification
    response["settings"] = settings
    response["bills"] = {room: round(max(0, float(energy_data[room].get("energy", 0))) * settings["tariff"], 2) for room in DEVICE_NAMES}
    response["total_bill"] = round(sum(response["bills"].values()), 2)
    response["esp32_connected"] = (
        last_esp32_seen is not None
        and time.monotonic() - last_esp32_seen < ESP32_TIMEOUT_SECONDS
    )
    response["esp32_last_seen_seconds_ago"] = (
        round(time.monotonic() - last_esp32_seen, 1)
        if last_esp32_seen is not None else None
    )

    return jsonify(response)


# =====================================================
# USER TRIP DECISION
# =====================================================

@app.route("/api/trip-decision", methods=["POST"])
def trip_decision():

    try:

        data = request.get_json()

        room = data.get("room")
        action = data.get("action")

        print("============================================")
        print("TRIP DECISION")
        print("Room   :", room)
        print("Action :", action)
        print("============================================")

        if room not in ["room1", "room2", "room3"]:

            return jsonify({
                "success": False,
                "error": "Invalid room"
            }), 400

        if action not in ["allow", "deny"]:

            return jsonify({
                "success": False,
                "error": "Invalid action"
            }), 400

        # =============================================
        # ALLOW
        # =============================================

        if action == "allow":

            trip_commands[room] = "ALLOW"

            print(
                f"TRIP ALLOWED -> "
                f"Room {room.replace('room', '')}"
            )

        # =============================================
        # DENY
        # =============================================

        elif action == "deny":

            trip_commands[room] = "DENY"

            print(
                f"TRIP DENIED -> "
                f"Room {room.replace('room', '')}"
            )

        # =============================================
        # MARK ALERT AS HANDLED
        # =============================================

        energy_data[room]["pending"] = 0

        energy_data[room]["alert_handled"] = True

        print(
            f"{room} -> alert_handled = True"
        )

        print(
            "Current trip commands:",
            trip_commands
        )

        return jsonify({

            "success": True,

            "room": room,

            "action": action,

            "command": trip_commands[room]

        })

    except Exception as e:

        print(
            "Trip decision error:",
            e
        )

        return jsonify({

            "success": False,

            "error": str(e)

        }), 500

# =====================================================
# ESP32 POLLS FOR COMMAND
# =====================================================

@app.route("/api/trip-commands", methods=["GET"])
def get_trip_commands():

    print("============================================")
    print("ESP32 REQUESTED TRIP COMMANDS")
    print("Current commands:")
    print(trip_commands)
    print("============================================")

    return jsonify({
        "limits": settings["limits"],
        "room1": trip_commands.get("room1", "NONE").upper(),
        "room2": trip_commands.get("room2", "NONE").upper(),
        "room3": trip_commands.get("room3", "NONE").upper()
    })

@app.route("/api/trip-ack", methods=["POST"])
def trip_ack():

    try:

        data = request.get_json()

        room = data.get("room")
        action = data.get("action")

        print("============================================")
        print("ESP32 TRIP ACK")
        print("Room   :", room)
        print("Action :", action)
        print("============================================")

        if room in trip_commands:

            trip_commands[room] = "NONE"

        return jsonify({
            "success": True
        })

    except Exception as e:

        print("ACK error:", e)

        return jsonify({
            "success": False,
            "error": str(e)
        }), 500

# =====================================================
# RUN SERVER
# =====================================================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )