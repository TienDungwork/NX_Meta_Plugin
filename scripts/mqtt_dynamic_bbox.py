#!/usr/bin/env python3
"""
MQTT Dynamic Bounding Box Test
Mô phỏng AI model thực tế: số lượng người/xe thay đổi theo thời gian
"""

import paho.mqtt.client as mqtt
import json
import time
import random

BROKER = "192.168.1.215"
PORT = 1883
CAMERA_ID = "742b49df-51af-29e2-75e5-d179f1b2d74d"
TOPIC = f"vms/ai/detections/{CAMERA_ID}"

def generate_random_bbox():
    """Tạo bbox ngẫu nhiên"""
    x = random.uniform(0.1, 0.7)
    y = random.uniform(0.1, 0.7)
    width = random.uniform(0.1, 0.2)
    height = random.uniform(0.1, 0.2)
    return [x, y, width, height]

def send_dynamic_detections():
    """
    Gửi detections với số lượng thay đổi theo thời gian
    Mô phỏng:
    - 0-5s: 2 người
    - 5-10s: 1 người (1 người đi ra)
    - 10-15s: 3 người (2 người đi vào)
    - 15-20s: 1 xe
    - 20-25s: 2 người + 1 xe
    - Sau đó lặp lại
    """
    
    print("="*80)
    print("🎬 MQTT Dynamic Bounding Box Test")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print("\n📊 Mô phỏng AI thực tế:")
    print("   - Số lượng objects thay đổi theo thời gian")
    print("   - Giống như người/xe đi vào, đi ra khỏi khung hình")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_dynamic_bbox_sender")
    
    # Connect
    print("\n🔄 Connecting to broker...")
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    time.sleep(1)
    print("✅ Connected! Sending dynamic detections...\n")
    print("Press Ctrl+C to stop\n")
    
    frame_index = 0
    next_track_id = 1
    
    # Tạo track IDs cố định cho mỗi object để tracking
    person_tracks = {}
    car_tracks = {}
    
    try:
        while True:
            # Tính thời gian trong cycle (25 giây)
            cycle_time = (frame_index * 0.04) % 25  # 25 FPS = 0.04s/frame
            
            detections = []
            
            # 0-5s: 2 người
            if 0 <= cycle_time < 5:
                scenario = "2 người"
                if 1 not in person_tracks:
                    person_tracks[1] = generate_random_bbox()
                if 2 not in person_tracks:
                    person_tracks[2] = generate_random_bbox()
                
                # Cập nhật vị trí (di chuyển nhẹ)
                for track_id in [1, 2]:
                    bbox = person_tracks[track_id]
                    bbox[0] += random.uniform(-0.01, 0.01)
                    bbox[1] += random.uniform(-0.01, 0.01)
                    bbox[0] = max(0.1, min(0.8, bbox[0]))
                    bbox[1] = max(0.1, min(0.8, bbox[1]))
                    
                    detections.append({
                        "label": "person",
                        "confidence": 0.95,
                        "bbox": bbox,
                        "trackId": track_id
                    })
            
            # 5-10s: 1 người (person 2 đã đi ra)
            elif 5 <= cycle_time < 10:
                scenario = "1 người (1 người đi ra)"
                if 2 in person_tracks:
                    del person_tracks[2]
                
                if 1 in person_tracks:
                    bbox = person_tracks[1]
                    bbox[0] += random.uniform(-0.01, 0.01)
                    bbox[1] += random.uniform(-0.01, 0.01)
                    bbox[0] = max(0.1, min(0.8, bbox[0]))
                    bbox[1] = max(0.1, min(0.8, bbox[1]))
                    
                    detections.append({
                        "label": "person",
                        "confidence": 0.95,
                        "bbox": bbox,
                        "trackId": 1
                    })
            
            # 10-15s: 3 người (2 người mới đi vào)
            elif 10 <= cycle_time < 15:
                scenario = "3 người (2 người đi vào)"
                if 2 not in person_tracks:
                    person_tracks[2] = generate_random_bbox()
                if 3 not in person_tracks:
                    person_tracks[3] = generate_random_bbox()
                
                for track_id in [1, 2, 3]:
                    if track_id not in person_tracks:
                        person_tracks[track_id] = generate_random_bbox()
                    
                    bbox = person_tracks[track_id]
                    bbox[0] += random.uniform(-0.01, 0.01)
                    bbox[1] += random.uniform(-0.01, 0.01)
                    bbox[0] = max(0.1, min(0.8, bbox[0]))
                    bbox[1] = max(0.1, min(0.8, bbox[1]))
                    
                    detections.append({
                        "label": "person",
                        "confidence": 0.95,
                        "bbox": bbox,
                        "trackId": track_id
                    })
            
            # 15-20s: 1 xe (tất cả người đã đi ra)
            elif 15 <= cycle_time < 20:
                scenario = "1 xe (người đã đi hết)"
                person_tracks.clear()
                
                if 1 not in car_tracks:
                    car_tracks[1] = generate_random_bbox()
                
                bbox = car_tracks[1]
                bbox[0] += random.uniform(-0.02, 0.02)
                bbox[1] += random.uniform(-0.01, 0.01)
                bbox[0] = max(0.1, min(0.8, bbox[0]))
                bbox[1] = max(0.1, min(0.8, bbox[1]))
                
                detections.append({
                    "label": "car",
                    "confidence": 0.9,
                    "bbox": bbox,
                    "trackId": 101
                })
            
            # 20-25s: 2 người + 1 xe
            else:
                scenario = "2 người + 1 xe"
                if 1 not in person_tracks:
                    person_tracks[1] = generate_random_bbox()
                if 2 not in person_tracks:
                    person_tracks[2] = generate_random_bbox()
                if 1 not in car_tracks:
                    car_tracks[1] = generate_random_bbox()
                
                # Người
                for track_id in [1, 2]:
                    bbox = person_tracks[track_id]
                    bbox[0] += random.uniform(-0.01, 0.01)
                    bbox[1] += random.uniform(-0.01, 0.01)
                    bbox[0] = max(0.1, min(0.8, bbox[0]))
                    bbox[1] = max(0.1, min(0.8, bbox[1]))
                    
                    detections.append({
                        "label": "person",
                        "confidence": 0.95,
                        "bbox": bbox,
                        "trackId": track_id
                    })
                
                # Xe
                bbox = car_tracks[1]
                bbox[0] += random.uniform(-0.02, 0.02)
                bbox[1] += random.uniform(-0.01, 0.01)
                bbox[0] = max(0.1, min(0.8, bbox[0]))
                bbox[1] = max(0.1, min(0.8, bbox[1]))
                
                detections.append({
                    "label": "car",
                    "confidence": 0.9,
                    "bbox": bbox,
                    "trackId": 101
                })
            
            # Tạo message
            message = {
                "detections": detections
            }
            
            # Gửi message
            client.publish(TOPIC, json.dumps(message))
            
            # Log mỗi 25 frames (1 giây)
            if frame_index % 25 == 0:
                print(f"⏱️  {cycle_time:05.2f}s | {scenario:25s} | {len(detections)} objects")
            
            frame_index += 1
            time.sleep(1.0 / 25.0)  # 25 FPS
            
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping...")
        
        # Gửi empty detections để clear bbox
        print("🧹 Clearing bboxes...")
        client.publish(TOPIC, json.dumps({"detections": []}))
        time.sleep(0.2)
        
        client.loop_stop()
        client.disconnect()
        
        print(f"✅ Sent {frame_index} frames")
        print("="*80)

if __name__ == "__main__":
    send_dynamic_detections()
