#!/usr/bin/env python3
"""
Script test để gửi MQTT message với trường totalCount (số người vào)
để test hiển thị counter trên video stream
"""

import paho.mqtt.client as mqtt
import json
import time

BROKER = "103.9.156.56"
PORT = 1883
CAMERA_ID = "729e9f41-9c81-f825-9355-73b5b702ca4e"  # Thay đổi camera ID của bạn
TOPIC = f"vms/ai/detections/{CAMERA_ID}"
USERNAME = "atin"
PASSWORD = "team1@123#"

BBOX_WIDTH = 0.15
BBOX_HEIGHT = 0.15

def send_test_with_counter():
    """
    Gửi MQTT message với totalCount để test hiển thị "Số người vào: X"
    """
    
    print("="*80)
    print("🧪 MQTT Test Counter - Test hiển thị 'Số người vào'")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print(f"Camera ID: {CAMERA_ID}")
    print("\n📊 Test:")
    print("   - Gửi detections với trường 'totalCount'")
    print("   - Counter sẽ tăng dần từ 0 để test")
    print("   - Hiển thị 'Số người vào: X' trên video")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_test_counter_sender")
    client.username_pw_set(USERNAME, PASSWORD)
    
    # Connect
    print("\n🔄 Connecting to broker...")
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(1)
        print("✅ Connected!\n")
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return
    
    frame_index = 0
    total_count = 0  # Bắt đầu từ 0
    
    try:
        print("📤 Sending messages... (Press Ctrl+C to stop)\n")
        
        while True:
            # Tăng counter mỗi 50 frames (2 giây với 25 FPS)
            if frame_index % 50 == 0 and frame_index > 0:
                total_count += 1
                print(f"📈 Counter updated: {total_count}")
            
            # Create detections với totalCount
            message = {
                "totalCount": total_count,  # Trường totalCount ở root level
                "detections": [
                    {
                        "label": "person",
                        "confidence": 0.95,
                        "bbox": [
                            0.30,           # x: center-left
                            0.50,           # y: center
                            BBOX_WIDTH,     # width
                            BBOX_HEIGHT     # height
                        ],
                        "trackId": 1,
                        "name": "Person 1",
                        "color": "Green"
                    },
                    {
                        "label": "person",
                        "confidence": 0.90,
                        "bbox": [
                            0.60,           # x: center-right
                            0.50,           # y: center
                            BBOX_WIDTH,     # width
                            BBOX_HEIGHT     # height
                        ],
                        "trackId": 2,
                        "name": "Person 2",
                        "color": "Blue"
                    }
                ]
            }
            
            # Publish
            result = client.publish(TOPIC, json.dumps(message), qos=0)
            result.wait_for_publish()
            
            # Print status mỗi 25 frames (1 giây với 25 FPS)
            if frame_index % 25 == 0:
                print(f"📤 Frame {frame_index:4d} | Total Count: {total_count:3d} | "
                      f"Detections: {len(message['detections'])} objects")
            
            frame_index += 1
            
            # Send at ~25 FPS
            time.sleep(1.0 / 25.0)
            
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping...")
        
        # Send empty detections để clear
        print("🧹 Clearing detections...")
        empty_message = {
            "totalCount": total_count,  # Giữ nguyên counter
            "detections": []
        }
        client.publish(TOPIC, json.dumps(empty_message), qos=0).wait_for_publish()
        time.sleep(0.2)
    
    finally:
        try:
            client.loop_stop()
            client.disconnect()
            print("✅ Disconnected")
        except:
            pass
    
    print(f"\n✅ Sent {frame_index} frames")
    print(f"📊 Final counter: {total_count}")
    print("="*80 + "\n")


if __name__ == "__main__":
    send_test_with_counter()
