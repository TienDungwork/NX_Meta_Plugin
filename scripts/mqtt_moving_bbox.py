#!/usr/bin/env python3"

import paho.mqtt.client as mqtt
import json
import time
import math
from uuid import uuid4

BROKER = "192.168.1.215"
PORT = 1883
CAMERA_ID = "742b49df-51af-29e2-75e5-d179f1b2d74d"
TOPIC = f"vms/ai/detections/{CAMERA_ID}"

# Simulating fake generation constants
TRACK_LENGTH = 100
BBOX_WIDTH = 0.15
BBOX_HEIGHT = 0.15

def calculate_y_position(frame_index):
    """
    Calculate Y position like fake generation:
    y = max(0.0, 1.0 - height - (1.0 / TRACK_LENGTH) * (frameIndex % TRACK_LENGTH))
    
    Starts from bottom (1.0 - height) and moves up to 0.0
    """
    progress = (frame_index % TRACK_LENGTH) / TRACK_LENGTH
    y = max(0.0, 1.0 - BBOX_HEIGHT - progress)
    return y

def send_moving_detections():
    """Send moving bounding boxes like fake generation"""
    
    print("="*80)
    print("🎬 MQTT Moving Bounding Box Test")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print("\n📊 Simulation:")
    print("   - 2 objects: Person (left), Car (right)")
    print("   - Moving from BOTTOM to TOP")
    print("   - Like fake generation pattern")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_moving_bbox_sender")
    
    # Connect
    print("\n🔄 Connecting to broker...")
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    time.sleep(1)
    
    print("✅ Connected! Sending moving bboxes...\n")
    print("Press Ctrl+C to stop\n")
    
    frame_index = 0
    
    try:
        while True:
            # Calculate Y positions (moving from bottom to top)
            y_person = calculate_y_position(frame_index)
            y_car = calculate_y_position(frame_index + 10)  # Slightly offset
            
            # Create detections with moving positions
            detections = {
                "detections": [
                    {
                        "label": "smoke",
                        "confidence": 0.95,
                        "bbox": [
                            0.15,           # x: left side
                            y_person,       # y: moving from bottom to top
                            BBOX_WIDTH,     # width
                            BBOX_HEIGHT     # height
                        ],
                        "trackId": 1
                    },
                    {
                        "label": "Fire",
                        "confidence": 0.90,
                        "bbox": [
                            0.70,           # x: right side
                            y_car,          # y: moving from bottom to top
                            BBOX_WIDTH,     # width
                            BBOX_HEIGHT     # height
                        ],
                        "trackId": 2
                    }
                ]
            }
            
            # Publish
            result = client.publish(TOPIC, json.dumps(detections), qos=0)
            result.wait_for_publish()
            
            # Print status every 10 frames
            if frame_index % 10 == 0:
                cycle_progress = (frame_index % TRACK_LENGTH) / TRACK_LENGTH * 100
                print(f"📤 Frame {frame_index:4d} | Cycle: {cycle_progress:5.1f}% | "
                      f"Person Y: {y_person:.3f} | Car Y: {y_car:.3f}")
            
            frame_index += 1
            
            # Simulate 25 FPS
            time.sleep(1.0 / 25.0)
            
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping...")
        
        # Send EMPTY detections to clear bboxes in VMS
        print("🧹 Clearing bboxes...")
        empty_detections = {"detections": []}
        client.publish(TOPIC, json.dumps(empty_detections), qos=0).wait_for_publish()
        time.sleep(0.2)
    
    finally:
        # Cleanup - handle potential interrupts during cleanup
        try:
            client.loop_stop()
            client.disconnect()
        except:
            pass  # Ignore cleanup errors
    
    print(f"\n✅ Sent {frame_index} frames")
    print("="*80 + "\n")

if __name__ == "__main__":
    send_moving_detections()
