import cv2
import urllib.request
import numpy as np
import requests

# ESP32-CAM IP address


esp32_ip = 'http://192.168.33.159'  # Replace with your actual ESP32 IP address

# Function to send detected object to ESP32
def send_detected_object_to_esp32(object_name):
    url = esp32_ip + '/send-object'
    headers = {'Content-Type': 'text/plain'}
    try:
        response = requests.post(url, data=object_name, headers=headers)
        if response.status_code == 200:
            print(f"ESP32 Response: {response.text}")
        else:
            print(f"Failed to send object. Status code: {response.status_code}")
    except Exception as e:
        print(f"Error sending object to ESP32: {e}")

# Object detection program
url = 'http://192.168.33.223/cam-hi.jpg'
winName = 'ESP32 CAMERA'
cv2.namedWindow(winName, cv2.WINDOW_AUTOSIZE)

classNames = []
classFile = 'coco.names'
with open(classFile, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

configPath = 'ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt'
weightsPath = 'frozen_inference_graph.pb'

net = cv2.dnn_DetectionModel(weightsPath, configPath)
net.setInputSize(320, 320)
net.setInputScale(1.0 / 127.5)
net.setInputMean((127.5, 127.5, 127.5))
net.setInputSwapRB(True)

while True:
    # Fetch image from ESP32-CAM
    imgResponse = urllib.request.urlopen(url)
    imgNp = np.array(bytearray(imgResponse.read()), dtype=np.uint8)
    img = cv2.imdecode(imgNp, -1)  # Decode image

    img = cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)  # Rotate image if necessary

    # Object Detection
    classIds, confs, bbox = net.detect(img, confThreshold=0.5)

    if len(classIds) != 0:
        for classId, confidence, box in zip(classIds.flatten(), confs.flatten(), bbox):
            object_name = classNames[classId - 1]
            print(f"Detected: {object_name}")

            # Send detected object name to ESP32
            send_detected_object_to_esp32(object_name)

            # Draw bounding box and object label on the image
            cv2.rectangle(img, box, color=(0, 255, 0), thickness=3)
            cv2.putText(img, object_name, (box[0] + 10, box[1] + 30), 
                        cv2.FONT_HERSHEY_COMPLEX, 1, (0, 255, 0), 2)

    # Display the image with bounding boxes and labels
    cv2.imshow(winName, img)

    # Break the loop if ESC key is pressed
    tecla = cv2.waitKey(5) & 0xFF
    if tecla == 27:
        break

cv2.destroyAllWindows()
