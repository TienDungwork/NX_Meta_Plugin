#!/usr/bin/env python3
"""
Script test đơn giản chỉ gửi totalCount (không có detections)
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

def send_counter_only():
    """
    Chỉ gửi totalCount, không có detections
    """
    
    print("="*80)
    print("🧪 MQTT Test Counter Only - Chỉ test counter, không có detections")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print(f"Camera ID: {CAMERA_ID}")
    print("\n📊 Test:")
    print("   - Chỉ gửi trường 'totalCount'")
    print("   - Không có detections (empty array)")
    print("   - Counter sẽ tăng dần để test hiển thị")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_test_counter_only_sender")
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
        print("📤 Sending counter messages... (Press Ctrl+C to stop)\n")
        print("💡 Tip: Counter sẽ tăng mỗi 2 giây\n")
        
        while True:
            # Tăng counter mỗi 50 frames (2 giây với 25 FPS)
            if frame_index % 50 == 0 and frame_index > 0:
                total_count += 1
                print(f"📈 Counter: {total_count}")
            
            # Chỉ gửi totalCount, không có detections
            message = {
                "totalCount": total_count,
                "detections": []  # Empty detections
            }
            
            # Publish
            result = client.publish(TOPIC, json.dumps(message), qos=0)
            result.wait_for_publish()
            
            frame_index += 1
            
            # Send at ~25 FPS
            time.sleep(1.0 / 25.0)
            
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping...")
        print(f"📊 Final counter: {total_count}")
    
    finally:
        try:
            client.loop_stop()
            client.disconnect()
            print("✅ Disconnected")
        except:
            pass
    
    print("="*80 + "\n")


if __name__ == "__main__":
    send_counter_only()
