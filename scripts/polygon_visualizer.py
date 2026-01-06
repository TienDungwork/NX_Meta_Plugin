import paho.mqtt.client as mqtt
import json
import cv2
import numpy as np
from datetime import datetime
import threading
import time

# Cấu hình MQTT
MQTT_BROKER = "192.168.1.215"
MQTT_PORT = 1883
MQTT_REQUEST_TOPIC = "vms/roi/request"
MQTT_RESPONSE_TOPIC = "vms/roi/response"
MQTT_CLIENT_ID = "polygon_visualizer"

# Cấu hình Camera
CAMERA_ID = "{7e1069d3-593b-bfd2-3271-bbe924e03da2}"
RTSP_URL = "rtsp://ntiendung:T12345678@192.168.1.215:7001/7e1069d3-593b-bfd2-3271-bbe924e03da2"

# Polling interval (seconds)
POLYGON_REQUEST_INTERVAL = 3

# Global variables
current_polygons = []
polygons_lock = threading.Lock()
frame_count = 0
mqtt_client = None

def send_polygon_request(client):
    request = {"action": "get_polygon", "camera_id": CAMERA_ID}
    try:
        client.publish(MQTT_REQUEST_TOPIC, json.dumps(request))
        print(f"[{datetime.now()}] Request sent for camera: {CAMERA_ID}")
    except Exception as e:
        print(f"Error: {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT")
        client.subscribe(MQTT_RESPONSE_TOPIC)
        send_polygon_request(client)
    else:
        print(f"Connection failed: {rc}")

def on_message(client, userdata, msg):
    global current_polygons
    try:
        data = json.loads(msg.payload.decode())
        if data.get('camera_id') != CAMERA_ID:
            return
        print(f"\nResponse received - {len(data.get('polygons', []))} polygon(s)")
        with polygons_lock:
            current_polygons = data.get('polygons', [])
    except Exception as e:
        print(f"Error: {e}")

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print("MQTT disconnected")

def mqtt_thread():
    global mqtt_client
    mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.on_disconnect = on_disconnect
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"MQTT Error: {e}")

def polygon_request_thread():
    global mqtt_client
    time.sleep(3)
    while True:
        try:
            if mqtt_client and mqtt_client.is_connected():
                send_polygon_request(mqtt_client)
            time.sleep(POLYGON_REQUEST_INTERVAL)
        except:
            time.sleep(5)

def draw_polygons_on_frame(frame, polygons):
    h, w = frame.shape[:2]
    for polygon in polygons:
        if 'points' not in polygon:
            continue
        points = [[int(pt[0]*w), int(pt[1]*h)] for pt in polygon['points']]
        pts = np.array(points, dtype=np.int32)
        color_hex = polygon.get('color', '#e040fb').lstrip('#')
        r, g, b = int(color_hex[0:2], 16), int(color_hex[2:4], 16), int(color_hex[4:6], 16)
        color = (b, g, r)
        overlay = frame.copy()
        cv2.fillPoly(overlay, [pts], color)
        cv2.addWeighted(overlay, 0.3, frame, 0.7, 0, frame)
        cv2.polylines(frame, [pts], True, color, 3)
        for i, pt in enumerate(points):
            cv2.circle(frame, tuple(pt), 6, (0, 255, 255), -1)
        name = polygon.get('name', 'Unknown')
        cv2.putText(frame, name, (points[0][0], points[0][1]-25),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    return frame

def main():
    global frame_count
    print("="*80)
    print("POLYGON VISUALIZER - Request/Response Mode")
    print(f"Camera: {CAMERA_ID}")
    print(f"Request interval: {POLYGON_REQUEST_INTERVAL}s")
    print("="*80)
    
    threading.Thread(target=mqtt_thread, daemon=True).start()
    threading.Thread(target=polygon_request_thread, daemon=True).start()
    
    time.sleep(3)
    cap = cv2.VideoCapture(RTSP_URL)
    if not cap.isOpened():
        print("Cannot connect to RTSP")
        return
    
    print("RTSP connected")
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"Resolution: {width}x{height}\n")
    
    last_save = time.time()
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                time.sleep(1)
                cap = cv2.VideoCapture(RTSP_URL)
                continue
            
            frame_count += 1
            has_polygons = False
            with polygons_lock:
                if current_polygons:
                    frame = draw_polygons_on_frame(frame, current_polygons)
                    has_polygons = True
            
            info = f"Frame: {frame_count} | Polygons: {len(current_polygons)}"
            cv2.putText(frame, info, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(frame, datetime.now().strftime("%Y-%m-%d %H:%M:%S"), 
                       (10, height-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            
            if has_polygons and time.time() - last_save >= 5:
                filename = f"polygon_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
                cv2.imwrite(filename, frame)
                print(f"Saved: {filename}")
                last_save = time.time()
            
            time.sleep(0.03)
    except KeyboardInterrupt:
        print("\nStopping...")
    
    cap.release()
    print("Done!")

if __name__ == "__main__":
    main()
