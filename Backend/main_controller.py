import cv2
import requests
import threading
import queue
import time
from ultralytics import YOLO

# ==========================================
# CONFIGURATION
# ==========================================
# Trimmed the trailing slash to prevent double-slash routing issues (e.g., //control)
ESP32_IP = "http://192.168.1.38" 

MODEL_PATH = r"C:\Users\SARTHAK\OneDrive\Documents\SELF PROJECTS\ESP32 Rover YOLOv8\custom yolo object detection\Model\best.pt"
VIDEO_STREAM = "https://192.168.1.45:8080/video"

# Thread-safe communication queue
cmd_queue = queue.Queue()

# ==========================================
# BACKGROUND NETWORK WORKER THREAD
# ==========================================
def network_worker():
    """Processes commands out of the queue in the background.
       This prevents network delays from freezing the OpenCV camera stream."""
    print("[System] Background network thread active.")
    while True:
        cmd = cmd_queue.get()
        if cmd is None:
            break # Exit signal
        
        try:
            url = f"{ESP32_IP}/control?cmd={cmd}"
            # Short timeout keeps the queue moving quickly
            response = requests.get(url, timeout=0.4)
            print(f"[Laptop -> Rover] Sent: {cmd} (Status: {response.status_code})")
        except requests.exceptions.RequestException as e:
            print(f"[Network Error] Delayed or lost packet for '{cmd}': {e}")
        
        cmd_queue.task_done()
        time.sleep(0.01) # Small rest to protect CPU cycles

# Start the worker thread immediately as a daemon (closes automatically when script stops)
worker_thread = threading.Thread(target=network_worker, daemon=True)
worker_thread.start()

def send_cmd_async(cmd):
    """Pushes a command onto the queue without blocking the main loop execution."""
    cmd_queue.put(cmd)

# ==========================================
# MAIN INITIALIZATION
# ==========================================
print("[System] Initializing YOLOv8 model...")
model = YOLO(MODEL_PATH)

print("[System] Connecting to video stream...")
cap = cv2.VideoCapture(VIDEO_STREAM)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

frame_count = 0
last_result = None
person_detected_state = False  

# State tracking to filter out key-hold spamming
last_sent_movement_cmd = "" 

print("\n--- ROBUST LAPTOP CONTROL ONLINE ---")
print("Focus (click) the 'YOLO Detection' window to execute actions:")
print("  W, A, S, D : Manual Drive controls")
print("  Spacebar   : Manual Emergency Stop")
print("  T          : Toggle Turbo Mode")
print("  M          : Toggle Autonomous Mode")
print("  Q          : Safe Quit Application\n")

# ==========================================
# MAIN LOOP
# ==========================================
while True:
    ret, frame = cap.read()
    if not ret:
        print("[Warning] Frame dropped. Attempting recovery...")
        time.sleep(0.1)
        continue # Don't break out immediately; give the stream a chance to reconnect

    frame_count += 1

    # Execute inference every 3 frames to maintain high frame rates
    if frame_count % 3 == 0:
        results = model(frame, imgsz=320, verbose=False)
        last_result = results[0].plot()

        # Deduce target presence state
        current_detection = len(results[0].boxes) > 0

        # Stateful edge triggers: Only queue requests when presence changes
        if current_detection != person_detected_state:
            person_detected_state = current_detection
            if person_detected_state:
                send_cmd_async("PERSON_DETECTED")
            else:
                send_cmd_async("CLEAR")

    # Display window rendering
    if last_result is not None:
        cv2.imshow("YOLO Detection & Laptop Control", last_result)
    else:
        cv2.imshow("YOLO Detection & Laptop Control", frame)

    # Keyboard Listener (checks for a keystroke within a 1ms window)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('q'):
        break
        
    # Manual Control Directives with Spam Safeguards
    elif key == ord('w'):
        if last_sent_movement_cmd != "F":
            send_cmd_async("F")
            last_sent_movement_cmd = "F"
    elif key == ord('s'):
        if last_sent_movement_cmd != "B":
            send_cmd_async("B")
            last_sent_movement_cmd = "B"
    elif key == ord('a'):
        if last_sent_movement_cmd != "L":
            send_cmd_async("L")
            last_sent_movement_cmd = "L"
    elif key == ord('d'):
        if last_sent_movement_cmd != "R":
            send_cmd_async("R")
            last_sent_movement_cmd = "R"
    elif key == ord(' '):
        if last_sent_movement_cmd != "S":
            send_cmd_async("S")
            last_sent_movement_cmd = "S"
            
    # System Toggle Operations
    elif key == ord('t'):
        send_cmd_async("TURBO")
        # Brief sleep clears user keystroke bounce
        time.sleep(0.15) 
    elif key == ord('m'):
        send_cmd_async("AUTO")
        last_sent_movement_cmd = "" # Wipe history to re-enable manual takeover anytime
        time.sleep(0.15)

# Clean terminations
print("[System] Shutting down application gracefully...")
cap.release()
cv2.destroyAllWindows()