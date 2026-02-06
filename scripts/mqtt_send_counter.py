#!/usr/bin/env python3
"""
Script để gửi people counter (totalCount) lên MQTT topic riêng
Topic: vms/ai/counter/{CAMERA_ID}
"""

import paho.mqtt.client as mqtt
import json
import time

BROKER = "103.9.158.149"
PORT = 1883
CAMERA_ID = "729e9f41-9c81-f825-9355-73b5b702ca4e"  # Thay đổi camera ID của bạn
TOPIC = f"vms/ai/counter/{CAMERA_ID}"  # Topic riêng cho counter

def send_counter():
    """
    Gửi people counter lên topic riêng
    """
    
    print("="*80)
    print("📊 MQTT Send People Counter - Gửi counter lên topic riêng")
    print("="*80)
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print(f"Camera ID: {CAMERA_ID}")
    print("\n📊 Test:")
    print("   - Gửi totalCount lên topic riêng: vms/ai/counter/{CAMERA_ID}")
    print("   - Counter sẽ tăng dần từ 0 để test")
    print("   - Chỉ hiển thị text, không tạo Best Shot packets")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_counter_sender")

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
                "totalCount": total_count
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
    send_counter()
