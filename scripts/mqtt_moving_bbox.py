#!/usr/bin/env python3

import paho.mqtt.client as mqtt
import json
import time

BROKER = "localhost"
PORT = 1883
CAMERA_ID = "0fdba7df-64c1-ebd4-9e73-677f2d77f211"
TOPIC = f"vms/ai/detections/{CAMERA_ID}"
# USERNAME = "admin"
# PASSWORD = "Ab@123456"

# Publish rate and one full bottom→top cycle length (frames).
FPS = 20
TRACK_LENGTH = 100
BBOX_WIDTH = 0.15
BBOX_HEIGHT = 0.15


def calculate_y_position(frame_index: int) -> float:
    """
    Normalized top-left Y: bbox moves straight from bottom of the frame to the top.

    progress 0   → y = 1.0 - height (box on bottom edge)
    progress → 1 → y → 0.0 (box at top)
    """
    progress = (frame_index % TRACK_LENGTH) / float(TRACK_LENGTH)
    y_at_bottom = 1.0 - BBOX_HEIGHT
    y_at_top = 0.0
    return y_at_bottom + (y_at_top - y_at_bottom) * progress

def send_moving_detections():
    """Send moving bounding boxes like fake generation"""
    
    print("="*80)
    print("🎬 MQTT Moving Bounding Box Test")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print("\n📊 Simulation:")
    print("   - 2 objects: Person (left), Car (right)")
    print("   - Moving from BOTTOM to TOP (linear)")
    print(f"   - Publish rate: {FPS} FPS")
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
            y_vehicle = calculate_y_position(frame_index + 10)  # slightly offset

            # Create detections (face + vehicle)
            detections = {
                "detections": [
                    {
                        # Use canonical labels so VMS taxonomy mapping works.
                        "label": "face",
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
                        "label": "vehicle",
                        "confidence": 0.90,
                        "bbox": [
                            0.70,           # x: right side
                            y_vehicle,      # y: moving from bottom to top
                            BBOX_WIDTH,     # width
                            BBOX_HEIGHT     # height
                        ],
                        "trackId": 2
                    },
                ]
            }
            
            # Publish
            result = client.publish(TOPIC, json.dumps(detections), qos=0)
            result.wait_for_publish()
            
            # Print status every 10 frames
            if frame_index % 10 == 0:
                cycle_progress = (frame_index % TRACK_LENGTH) / TRACK_LENGTH * 100
                print(
                    f"📤 Frame {frame_index:4d} | Cycle: {cycle_progress:5.1f}% | "
                    f"Face Y: {y_person:.3f} | Vehicle Y: {y_vehicle:.3f}"
                )
            
            frame_index += 1

            time.sleep(1.0 / FPS)
            
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
