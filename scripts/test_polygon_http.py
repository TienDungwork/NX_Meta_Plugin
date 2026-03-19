import requests
import json
import sys

HTTP_SERVER = "http://192.168.1.196:8090"
CAMERA_IDS = {
    "1": "{0392f587-7ba3-6f0e-a193-621bdc515e16}"
}

def request_polygon(camera_id):
    url = f"{HTTP_SERVER}/polygon"
    params = {"camera_id": camera_id}
    try:
        response = requests.get(url, params=params, timeout=5)
        
        print(f"\nStatus Code: {response.status_code}")
        print(f"Response Headers: {dict(response.headers)}")
        print(f"\nResponse Body:")
        
        try:
            data = response.json()
            print(json.dumps(data, indent=2))
            
            if "polygons" in data and len(data["polygons"]) > 0:
                for i, polygon in enumerate(data["polygons"]):
                    print(f"  Polygon {i+1}: {polygon['name']}")
                    print(f"    Points: {len(polygon['points'])}")
                    print(f"    Color: {polygon.get('color', 'N/A')}")
        except json.JSONDecodeError:
            print(response.text)
            
    except requests.exceptions.RequestException as e:
        return False
    
    return True

def main():
    if len(sys.argv) > 1:
        camera_id = sys.argv[1]
        request_polygon(camera_id)
    else:
        print("\nAvailable cameras:")
        for key, camera_id in CAMERA_IDS.items():
            print(f"  {key}: {camera_id}")
        
        choice = input("\nSelect camera (1 or 2): ").strip()
        
        if choice in CAMERA_IDS:
            request_polygon(CAMERA_IDS[choice])
        else:
            print("Invalid choice")

if __name__ == "__main__":
    main()
