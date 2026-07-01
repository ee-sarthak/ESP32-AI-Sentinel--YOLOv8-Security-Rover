# ESP32 + YOLOv8 Surveillance Rover

A custom-built security rover powered by an ESP32 and a YOLOv8 person-detection model. The rover streams video from an onboard IP camera to a laptop, which runs real-time person detection and sends movement/alert commands back to the ESP32 over Wi-Fi. The ESP32 also hosts a lightweight web dashboard for manual driving and runs an autonomous obstacle-avoidance mode using ultrasonic sensors and a sweeping radar servo.

**Demo video:** https://youtu.be/L-eqcJygF60

## Features

- **Live person detection** — a custom YOLOv8 model (trained via Roboflow) scans the camera feed and flags detections in real time.
- **Web dashboard** — the ESP32 serves an async, lightweight control panel for manual driving (WASD-style commands).
- **Autonomous obstacle avoidance** — front/rear ultrasonic sensors plus a sweeping radar servo let the rover navigate around obstacles on its own.
- **Hardware emergency stop** — an IR obstacle sensor triggers an immediate, firmware-level stop independent of any software command.
- **Audible alerts** — an active buzzer sounds when a person is detected.
- **Non-blocking command pipeline** — the detection script uses threading and a queue so sending commands to the ESP32 never lags the OpenCV video feed.

## How it works

1. An IP camera streams video to a laptop.
2. The laptop runs `person_detector.py`, which uses `ultralytics` YOLO to detect people in each frame.
3. On detection, a command (`PERSON_DETECTED`, or a movement command like `W`/`A`/`S`/`D`) is pushed onto a queue and sent asynchronously over HTTP to the ESP32, so detection and networking never block the video loop.
4. The ESP32 firmware receives the command and either drives the motors, sounds the buzzer, or both.
5. When idle, the ESP32 can run in autonomous mode, sweeping its radar servo and using the ultrasonic sensors to steer around obstacles.
6. If the IR sensor detects an obstacle directly ahead, it triggers a hardware-level emergency stop regardless of what the ESP32's main loop is doing.

## Repository contents

| File | Description |
|---|---|
| `person_detector.py` | Backend script (runs on your laptop). Uses `ultralytics` YOLO on an IP camera stream, and asynchronously sends movement/alert commands to the ESP32 via threading + a queue. |
| `rover_firmware.ino` | ESP32 Arduino firmware. Hosts the async web dashboard, handles manual driving, autonomous obstacle-avoidance mode, and the IR-based hardware emergency stop. |
| `yolo_test.py` | Standalone script for testing the custom Roboflow-trained YOLO model against static images, video files, or a local webcam. |

*(Rename the files above to match your actual filenames in the repo.)*

## Hardware

- ESP32 dev board
- 2x MX1508 motor drivers
- 3x DC motors (2 active front motors; 1 rear motor wired for future use)
- 1x servo motor (front-mounted radar sweep)
- 2x HC-SR04 ultrasonic sensors (front and rear)
- 1x IR obstacle sensor (hardware emergency stop)
- 1x active buzzer
- IP camera (for the video feed used by the detection script)

## Power distribution

The battery's positive and negative terminals are each split three ways:

| Rail | Goes to |
|---|---|
| Positive (V+) | MX1508 #1, MX1508 #2, ESP32 (5V) |
| Negative (GND) | MX1508 #1, MX1508 #2, ESP32 (GND) |

All boards share a common ground — this is required for the ESP32's GPIO signals to reliably control the motor drivers.

## Pin connections

**Front motor driver (MX1508 #1)**
| Pin | ESP32 GPIO |
|---|---|
| IN1 | 25 |
| IN2 | 26 |
| IN3 | 27 |
| IN4 | 14 |

Motor A → front right motor · Motor B → front left motor

**Rear motor driver (MX1508 #2) — wired for future use**
| Pin | ESP32 GPIO |
|---|---|
| IN1 | 21 |
| IN2 | 22 |

Motor A → rear motor

**Radar servo**
| Pin | Connection |
|---|---|
| Signal | GPIO 33 |
| Power / GND | 5V / GND |

**Front HC-SR04 (ultrasonic)**
| Pin | ESP32 GPIO |
|---|---|
| Trig | 18 |
| Echo | 19 |

**Rear HC-SR04 (ultrasonic)**
| Pin | ESP32 GPIO |
|---|---|
| Trig | 16 |
| Echo | 17 |

**IR obstacle sensor (emergency stop)**
| Pin | Connection |
|---|---|
| Signal | GPIO 32 |
| Power / GND | 5V / GND |

**Active buzzer**
| Pin | Connection |
|---|---|
| Signal | GPIO 13 |
| GND | GND |

## Setup

### 1. Flash the ESP32

1. Open `rover_firmware.ino` in the Arduino IDE (or PlatformIO).
2. Install any required async web server / servo / ultrasonic libraries.
3. Update your Wi-Fi credentials in the firmware.
4. Select your ESP32 board and port, then upload.
5. Note the IP address printed to Serial once it connects — you'll need it for the detection script.

### 2. Set up the laptop-side detection script

```bash
pip install ultralytics opencv-python requests
```

1. Point the script at your IP camera's stream URL.
2. Set the ESP32's IP address so commands can be sent over HTTP.
3. Load your custom Roboflow-trained YOLO model weights.
4. Run the script:

```bash
python person_detector.py
```

### 3. Test the model independently (optional)

```bash
python yolo_test.py --source path/to/image_or_video_or_webcam
```

### 4. Drive it

- Open the ESP32's IP address in a browser to access the manual-driving web dashboard.
- Or let it run in autonomous mode when idle.
- Walk in front of the camera to see person-detection trigger the buzzer.

## Roadmap

- [ ] Wire up and activate the rear motor for full 3-motor drive
- [ ] Tune autonomous obstacle-avoidance thresholds
- [ ] Add a live snapshot of detections to the web dashboard

## License

Add your preferred license here (e.g. MIT).
