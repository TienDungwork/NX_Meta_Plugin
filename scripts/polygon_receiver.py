#!/usr/bin/env python3
"""
Script nhận thông tin Polygon từ MQTT và hiển thị
Topic: vms/roi/polygon
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

# Cấu hình MQTT
MQTT_BROKER = "192.168.1.215"
MQTT_PORT = 1883
MQTT_TOPIC = "vms/roi/polygon"
MQTT_CLIENT_ID = "polygon_receiver"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[{datetime.now()}] Kết nối MQTT Broker thành công!")
        print(f"Listening on topic: {MQTT_TOPIC}")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"[{datetime.now()}] Kết nối thất bại, code: {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        print(f"\n{'='*80}")
        print(f"[{datetime.now()}] POLYGONS UPDATED")
        print(f"{'='*80}")
        
        # Parse JSON
        data = json.loads(payload)
        
        print(f"\nEvent: {data.get('event', 'N/A')}")
        print(f"Timestamp: {data.get('timestamp', 'N/A')}")
        
        # Hiển thị từng polygon
        if 'polygons' in data and data['polygons']:
            polygons = data['polygons']
            print(f"\n🔷 Drawn Polygons: {len(polygons)}")
            
            for i, polygon in enumerate(polygons, 1):
                print(f"\n{'─'*80}")
                print(f"[{i}] {polygon.get('name', 'Unknown')}")
                print(f"{'─'*80}")
                
                if 'points' in polygon:
                    points = polygon['points']
                    print(f"Vertices: {len(points)} points")
                    
                    for j, point in enumerate(points, 1):
                        print(f"   P{j}: ({point[0]:.4f}, {point[1]:.4f})")
                    
                    # Tính bounding box
                    if points:
                        x_coords = [p[0] for p in points]
                        y_coords = [p[1] for p in points]
                        width = max(x_coords) - min(x_coords)
                        height = max(y_coords) - min(y_coords)
                        
                        print(f"\nBounding Box:")
                        print(f"   Top-Left:     ({min(x_coords):.4f}, {min(y_coords):.4f})")
                        print(f"   Bottom-Right: ({max(x_coords):.4f}, {max(y_coords):.4f})")
                        print(f"   Size:         {width:.4f} x {height:.4f}")
                
                print(f"\nColor: {polygon.get('color', 'N/A')}")
                label = polygon.get('label', '')
                if label:
                    print(f" Label: {label}")
                print(f" Visible: {polygon.get('showOnCamera', False)}")
        else:
            print(f"\n No polygons drawn")
        
        
    except json.JSONDecodeError as e:
        print(f"JSON Parse Error: {e}")
        print(f"Raw: {payload[:200]}...")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

# Callback khi ngắt kết nối
def on_disconnect(client, userdata, rc):
    if rc != 0:
        print(f"[{datetime.now()}]  Mất kết nối MQTT! Đang thử kết nối lại...")

def main():
    print("="*80)
    print("POLYGON RECEIVER - MQTT CLIENT")
    print("="*80)
    print(f"MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"Topic: {MQTT_TOPIC}")
    print(f"Client ID: {MQTT_CLIENT_ID}")
    print("="*80)
    
    # Tạo MQTT client
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    
    # Gán callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        # Kết nối tới broker
        print(f"\nĐang kết nối tới {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        # Chạy loop
        print("Đang lắng nghe messages... (Ctrl+C để thoát)\n")
        client.loop_forever()
        
    except KeyboardInterrupt:
        print("\n\nĐang dừng polygon receiver...")
        client.disconnect()
        print("Đã dừng!")
    except Exception as e:
        print(f"\nLỗi: {e}")
        client.disconnect()

if __name__ == "__main__":
    main()
