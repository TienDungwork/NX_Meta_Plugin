import paho.mqtt.client as mqtt
import json
import time

BROKER = "192.168.1.215"
PORT = 1883
CAMERA_ID = "742b49df-51af-29e2-75e5-d179f1b2d74d"
TOPIC = f"vms/ai/detections/{CAMERA_ID}"

BBOX_WIDTH = 0.11
BBOX_HEIGHT = 0.1

PERSON_X = 0.50
PERSON_Y = 0.50

CAR_X = 0.70
CAR_Y = 0.50

def send_static_detections():
    
    print("="*80)
    print("MQTT Static Bounding Box Test")
    print(f"Broker: {BROKER}:{PORT}")
    print(f"Topic:  {TOPIC}")
    print("\nStatic Objects (CỐ ĐỊNH - KHÔNG DI CHUYỂN):")
    print(f"   - Person at (x={PERSON_X}, y={PERSON_Y}) - LEFT side")
    print(f"   - Car at (x={CAR_X}, y={CAR_Y}) - RIGHT side")
    print("\n" + "="*80)
    
    # Create MQTT client
    client = mqtt.Client(client_id="vms_static_bbox_sender")
    
    # Connect
    print("\nConnecting to broker...")
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    time.sleep(1)
    
    
    frame_index = 0
    
    try:
        while True:
            # Create detections with FIXED positions (vị trí cố định)
            detections = {
                "detections": [
                    {
                        "label": "person",
                        "confidence": 0.9,
                        "bbox": [
                            PERSON_X,       
                            PERSON_Y,       
                            BBOX_WIDTH,     
                            BBOX_HEIGHT     
                        ],
                        "trackId": 1
                    },
                    {
                        "label": "car",
                        "confidence": 0.90,
                        "bbox": [
                            CAR_X,          
                            CAR_Y,          
                            BBOX_WIDTH,     
                            BBOX_HEIGHT    
                        ],
                        "trackId": 2
                    }
                ]
            }
            
            # Publish
            result = client.publish(TOPIC, json.dumps(detections), qos=0)
            result.wait_for_publish()
            
            # Print status every 20 frames
            if frame_index % 20 == 0:
                print(f"Frame {frame_index:4d} | "
                      f"Person: ({PERSON_X}, {PERSON_Y}) | "
                      f"Car: ({CAR_X}, {CAR_Y}) - STATIC (CỐ ĐỊNH)")
            
            frame_index += 1
            
            # Send at ~25 FPS
            time.sleep(1.0 / 25.0)
            
    except KeyboardInterrupt:
        print("\n\nStopping...")
        
        empty_detections = {"detections": []}
        client.publish(TOPIC, json.dumps(empty_detections), qos=0).wait_for_publish()
        time.sleep(0.2)
    
    finally:
        try:
            client.loop_stop()
            client.disconnect()
        except:
            pass
    

if __name__ == "__main__":
    send_static_detections()
