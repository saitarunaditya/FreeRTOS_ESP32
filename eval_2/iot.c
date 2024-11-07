#include <WiFi.h>
    #include <HTTPClient.h>
    #include <Base64.h>
    #include <TinyGPS++.h>
    #include <AsyncTCP.h>
    #include <ESPAsyncWebServer.h>
    #include <SPIFFS.h>
    #include <ArduinoJson.h>
    #include <TimeLib.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <freertos/queue.h>
    #include <freertos/semphr.h>
    
    // Configuration Constants
    // WiFi Credentials
    static const char *ssid = "Lr10";
    static const char *password = "leonal123";
    
    // Twilio Credentials
    static const char *account_sid = "ACfa4ecc553a33093f0443b1ef2838d76e";
    static const char *auth_token = "f34cbb6aff72cba0a1ce917a2083b5de";
    static const char *from_number = "+14788453183";
    static const char *to_number = "+918248731433";
    
    // Task Stack Sizes
    #define SENSOR_STACK_SIZE 4096
    #define COMM_STACK_SIZE 4096
    #define WEB_STACK_SIZE 4096
    #define STACK_SIZE 2048
    
    // Task Periods (ms) and Priorities (adjusted for RMS and EDF)
#define SENSOR_PERIOD pdMS_TO_TICKS(100)            // Sensor task period = 100 ms
#define OBJECT_DETECTION_PERIOD pdMS_TO_TICKS(200)  // Object detection task period = 200 ms
#define COMMUNICATION_PERIOD pdMS_TO_TICKS(300)     // Communication task period = 300 ms
#define WEB_UPDATE_PERIOD pdMS_TO_TICKS(500)        // Web update task period = 500 ms

// Task Priorities for RMS (higher priority for shorter period)
#define SENSOR_PRIORITY (configMAX_PRIORITIES - 1)     // Highest priority for sensor task (shortest period)
#define OBJECT_DETECT_PRIORITY (configMAX_PRIORITIES - 2) // High priority for object detection task (short period)
#define COMM_PRIORITY (configMAX_PRIORITIES - 3)       // Medium priority for communication task (longer period)
#define WEB_PRIORITY (configMAX_PRIORITIES - 4)        // Lowest priority for web update task (longest period)

// For EDF (we'll simulate dynamic deadlines by adjusting task priorities in each iteration)
#define EDF_PRIORITY_OFFSET (configMAX_PRIORITIES - 5)  // For EDF, we dynamically adjust the priority each time



// Define ceiling priorities for each semaphore
#define SENSOR_MUTEX_CEILING_PRIORITY    (SENSOR_PRIORITY)
#define OBJECT_DETECTION_MUTEX_CEILING   (OBJECT_DETECTION_PRIORITY)
#define COMM_MUTEX_CEILING_PRIORITY      (COMM_PRIORITY)

// Define mutexes
xSemaphoreHandle sensorMutex;
xSemaphoreHandle objectDetectionMutex;
xSemaphoreHandle commMutex;

// Create the mutexes and assign the ceiling priorities
sensorMutex = xSemaphoreCreateMutex();
vSemaphoreCreateBinary(objectDetectionMutex);
vSemaphoreCreateBinary(commMutex);

// Set ceiling priority for each mutex
vSemaphoreSetMutexCeiling(sensorMutex, SENSOR_MUTEX_CEILING_PRIORITY);
vSemaphoreSetMutexCeiling(objectDetectionMutex, OBJECT_DETECTION_MUTEX_CEILING);
vSemaphoreSetMutexCeiling(commMutex, COMM_MUTEX_CEILING_PRIORITY);



    // Pin Definitions
    #define RXD2 16
    #define TXD2 17
    #define GPS_BAUD 9600
    #define BUZZ 19
    #define BUTTON_PIN_GPS 4
    #define BUTTON_PIN_ONOFF 5
    #define WATER_SENSOR_PIN 34
    #define TRIG_PIN1 27
    #define ECHO_PIN1 26
    #define TRIG_PIN2 22
    #define ECHO_PIN2 23
    
    // Global Variables and Objects
    AsyncWebServer server(80);
    TinyGPSPlus gps;
    HardwareSerial gpsSerial(2);
    
    // Task Handles
    TaskHandle_t sensorTaskHandle = NULL;
    TaskHandle_t objectDetectionTaskHandle = NULL;
    TaskHandle_t communicationTaskHandle = NULL;
    TaskHandle_t webUpdateTaskHandle = NULL;
    
    // FreeRTOS Queues
    QueueHandle_t sensorQueue;
    QueueHandle_t alertQueue;
    
    // FreeRTOS Semaphores
    SemaphoreHandle_t sensorMutex;
    SemaphoreHandle_t webMutex;
    SemaphoreHandle_t commMutex;
    SemaphoreHandle_t objectDetectionMutex;
    
    // System State
    bool systemOn = false;
    bool lastButtonState = HIGH;
    
    // Data Structures
    struct SensorData {
        float waterLevel;
        float distance1;
        float distance2;
        float temperature;
        String lastDetectedObject;
        double latitude;
        double longitude;
        bool waterDetected;
        bool objectDetected;
        String timestamp;
    } sensorData;
    
    struct Alert {
        bool waterDetected;
        bool objectDetected;
        double latitude;
        double longitude;
    };
    
    // HTML Template
    const char index_html[] PROGMEM = R"rawliteral(
    <!DOCTYPE HTML>
    <html>
    <head>
        <title>ESP32 Security Monitor</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body { 
                font-family: Arial; 
                margin: 0;
                padding: 20px;
                background-color: #f0f0f0;
            }
            .dashboard {
                max-width: 1000px;
                margin: 0 auto;
                background: white;
                padding: 20px;
                border-radius: 10px;
                box-shadow: 0 0 10px rgba(0,0,0,0.1);
            }
            .grid-container {
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
                gap: 20px;
                margin-top: 20px;
            }
            .card {
                background: #ffffff;
                border-radius: 8px;
                padding: 15px;
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }
            .alert {
                background-color: #ff4444;
                color: white;
                padding: 15px;
                margin: 10px 0;
                border-radius: 5px;
                display: none;
            }
            .value {
                font-size: 24px;
                font-weight: bold;
                color: #2196F3;
            }
            .label {
                color: #666;
                font-size: 14px;
            }
            #map {
                height: 300px;
                margin-top: 20px;
                border-radius: 8px;
            }
            .timestamp {
                text-align: right;
                color: #666;
                font-size: 12px;
                margin-top: 20px;
            }
        </style>
    </head>
    <body>
        <div class="dashboard">
            <h1>ESP32 Security Monitor</h1>
            
            <div class="alert" id="waterAlert">
                ⚠️ Water Detection Alert! Immediate attention required.
            </div>
            <div class="alert" id="objectAlert">
                ⚠️ Object Detection Alert! Check surveillance system.
            </div>
            
            <div class="grid-container">
                <div class="card">
                    <div class="label">Water Level</div>
                    <div class="value"><span id="waterLevel">0</span>%</div>
                </div>
                
                <div class="card">
                    <div class="label">Distance Sensors</div>
                    <div class="value">
                        <span id="distance1">0</span>cm / <span id="distance2">0</span>cm
                    </div>
                </div>
                
                <div class="card">
                    <div class="label">Temperature</div>
                    <div class="value"><span id="temperature">0</span>°C</div>
                </div>
                
                <div class="card">
                    <div class="label">Last Detected Object</div>
                    <div class="value"><span id="lastObject">None</span></div>
                </div>
            </div>
            
            <div class="card">
                <div class="label">GPS Location</div>
                <div id="map"></div>
                <div class="value" style="font-size: 16px;">
                    Coordinates: <span id="coordinates">Waiting for GPS...</span>
                </div>
            </div>
            
            <div class="timestamp">
                Last Updated: <span id="timestamp">--</span>
            </div>
        </div>
    
        <script>
            let map, marker;
    
            function initMap() {
                map = new google.maps.Map(document.getElementById('map'), {
                    zoom: 15,
                    center: { lat: 0, lng: 0 }
                });
                marker = new google.maps.Marker({ map: map });
            }
    
            function updateData() {
                fetch('/data')
                    .then(response => response.json())
                    .then(data => {
                        document.getElementById('waterLevel').innerHTML = data.waterLevel.toFixed(1);
                        document.getElementById('distance1').innerHTML = data.distance1.toFixed(1);
                        document.getElementById('distance2').innerHTML = data.distance2.toFixed(1);
                        document.getElementById('temperature').innerHTML = data.temperature.toFixed(1);
                        document.getElementById('lastObject').innerHTML = data.lastDetectedObject || 'None';
                        document.getElementById('timestamp').innerHTML = data.timestamp;
                        
                        document.getElementById('waterAlert').style.display = 
                            data.waterDetected ? 'block' : 'none';
                        document.getElementById('objectAlert').style.display = 
                            data.objectDetected ? 'block' : 'none';
                        
                        if (data.latitude && data.longitude) {
                            const position = { lat: data.latitude, lng: data.longitude };
                            marker.setPosition(position);
                            map.setCenter(position);
                            document.getElementById('coordinates').innerHTML = 
                                `${data.latitude.toFixed(6)}, ${data.longitude.toFixed(6)}`;
                        }
                    });
            }
    
            setInterval(updateData, 1000);
        </script>
        <script async defer
            src="https://maps.googleapis.com/maps/api/js?key=YOUR_GOOGLE_MAPS_API_KEY&callback=initMap">
        </script>
    </body>
    </html>
    )rawliteral";
    
    // Function Prototypes
    void sendSMS(String message);
    void setupWebServer();
    void sensorTask(void *pvParameters);
    void objectDetectionTask(void *pvParameters);
    void communicationTask(void *pvParameters);
    void webUpdateTask(void *pvParameters);
   
  
    
    // EDF Task: Communication Task
void communicationTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Here we dynamically adjust priority based on deadline proximity (EDF)
        TickType_t current_time = xTaskGetTickCount();
        TickType_t remaining_time = COMMUNICATION_PERIOD - (current_time % COMMUNICATION_PERIOD);
        
        if (remaining_time < COMMUNICATION_PERIOD / 2) {
            // Increase priority if deadline is close (EDF behavior)
            vTaskPrioritySet(communicationTaskHandle, EDF_PRIORITY_OFFSET + 1);
        } else {
            // Default priority if deadline is far
            vTaskPrioritySet(communicationTaskHandle, COMM_PRIORITY);
        }

        if(xSemaphoreTake(commMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // GPS Reading
            while (gpsSerial.available() > 0) {
                if (gps.encode(gpsSerial.read())) {
                    if (gps.location.isUpdated()) {
                        sensorData.latitude = gps.location.lat();
                        sensorData.longitude = gps.location.lng();
                    }
                }
            }

            // Alert Handling
            if (sensorData.waterDetected || sensorData.objectDetected) {
                String message = "Alert: ";
                if (sensorData.waterDetected) message += "Water detected! ";
                if (sensorData.objectDetected) message += "Object in proximity! ";
                if (gps.location.isValid()) {
                    message += "Location: https://maps.google.com/?q=" + 
                              String(sensorData.latitude, 6) + "," + 
                              String(sensorData.longitude, 6);
                }
                sendSMS(message);
            }
            xSemaphoreGive(commMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, COMMUNICATION_PERIOD);
    }
}

// RMS Task: Sensor Task
// Assuming other necessary setup like semaphore and task handles, etc.

void sensorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // Set priority based on RMS behavior (shorter period = higher priority)
    vTaskPrioritySet(sensorTaskHandle, SENSOR_PRIORITY);

    while(1) {
        // Apply Priority Ceiling Protocol (PCP): acquire mutexes in priority order
        if(xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Critical section for sensor reading

            // Water Sensor Reading
            int waterValue = analogRead(WATER_SENSOR_PIN);
            sensorData.waterLevel = (float)waterValue / 4095.0 * 100.0;
            sensorData.waterDetected = waterValue > 2000;
            
            // Ultrasonic Sensor 1
            digitalWrite(TRIG_PIN1, LOW);
            delayMicroseconds(2);
            digitalWrite(TRIG_PIN1, HIGH);
            delayMicroseconds(10);
            digitalWrite(TRIG_PIN1, LOW);
            long duration1 = pulseIn(ECHO_PIN1, HIGH);
            sensorData.distance1 = duration1 * 0.034 / 2;
            
            // Ultrasonic Sensor 2
            digitalWrite(TRIG_PIN2, LOW);
            delayMicroseconds(2);
            digitalWrite(TRIG_PIN2, HIGH);
            delayMicroseconds(10);
            digitalWrite(TRIG_PIN2, LOW);
            long duration2 = pulseIn(ECHO_PIN2, HIGH);
            sensorData.distance2 = duration2 * 0.034 / 2;
            
            sensorData.objectDetected = (sensorData.distance1 < 20 || sensorData.distance2 < 20);
            sensorData.temperature = 25.0 + (random(-10, 10) / 10.0); // Simulated temperature
            
            char timestamp[20];
            sprintf(timestamp, "%02d:%02d:%02d", hour(), minute(), second());
            sensorData.timestamp = String(timestamp);
            
            xSemaphoreGive(sensorMutex); // Release sensorMutex after reading
        }
        
        // Next critical section (object detection and communication) 
        // Apply Priority Ceiling Protocol: acquire mutexes in priority order
        if(xSemaphoreTake(objectDetectionMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Critical section for object detection processing
            // You can add object detection logic here if necessary.
            xSemaphoreGive(objectDetectionMutex); // Release objectDetectionMutex
        }

        if(xSemaphoreTake(commMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Critical section for communication tasks (e.g., sending sensor data)
            // Communication logic here.
            xSemaphoreGive(commMutex); // Release commMutex
        }

        // Priority inheritance might be triggered automatically by FreeRTOS 
        // if higher-priority tasks are waiting on these mutexes.

        vTaskDelayUntil(&xLastWakeTime, SENSOR_PERIOD);
    }
}

// RMS Task: Object Detection Task
void objectDetectionTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Set priority based on RMS behavior (shorter period = higher priority)
        vTaskPrioritySet(objectDetectionTaskHandle, OBJECT_DETECT_PRIORITY);
        
        if(xSemaphoreTake(objectDetectionMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if(sensorData.objectDetected) {
                digitalWrite(BUZZ, HIGH);
                delay(100);
                digitalWrite(BUZZ, LOW);
                
                sensorData.lastDetectedObject = "Unknown Object";
            }
            xSemaphoreGive(objectDetectionMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, OBJECT_DETECTION_PERIOD);
    }
}

// Web Update Task (No changes for scheduling as this is the lowest priority)
void webUpdateTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        if(xSemaphoreTake(webMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Update web interface data
            char timestamp[20];
            sprintf(timestamp, "%02d:%02d:%02d", hour(), minute(), second());
            sensorData.timestamp = String(timestamp);
            
            xSemaphoreGive(webMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, WEB_UPDATE_PERIOD);
    }
}

    
    // Send SMS Implementation
    void sendSMS(String message) {
        if (WiFi.status() != WL_CONNECTED) return;
        
        HTTPClient http;
        String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(account_sid) + "/Messages.json";
        http.begin(url);
        
        String auth = String(account_sid) + ":" + String(auth_token);
        String auth_base64 = base64::encode(auth);
        http.addHeader("Authorization", "Basic " + auth_base64);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        String postData = "To=" + String(to_number) + 
                         "&From=" + String(from_number) + 
                         "&Body=" + message;
        
        int httpResponseCode = http.POST(postData);
        http.end();
    }
    
    // Web Server Setup Implementation
    void setupWebServer() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", index_html);
        });
    
        server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
            String jsonResponse;
            StaticJsonDocument<512> doc;
            
            doc["waterLevel"] = sensorData.waterLevel;
            doc["distance1"] = sensorData.distance1;
            // Continuing the setupWebServer() function
            doc["distance2"] = sensorData.distance2;
            doc["temperature"] = sensorData.temperature;
            doc["lastDetectedObject"] = sensorData.lastDetectedObject;
            doc["latitude"] = sensorData.latitude;
            doc["longitude"] = sensorData.longitude;
            doc["waterDetected"] = sensorData.waterDetected;
            doc["objectDetected"] = sensorData.objectDetected;
            doc["timestamp"] = sensorData.timestamp;
            
            serializeJson(doc, jsonResponse);
            request->send(200, "application/json", jsonResponse);
        });
    
        server.begin();
    }

    
    // Button handling function
    void handleButtons() {
        // On/Off button
        bool currentButtonState = digitalRead(BUTTON_PIN_ONOFF);
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            systemOn = !systemOn;
            if (!systemOn) {
                digitalWrite(BUZZ, LOW);  // Turn off buzzer when system is turned off
            }
            delay(50);  // Simple debounce
        }
        lastButtonState = currentButtonState;
        
        // GPS button (for testing GPS functionality)
        if (digitalRead(BUTTON_PIN_GPS) == LOW) {
            if (gps.location.isValid()) {
                String message = "Current Location: https://maps.google.com/?q=" + 
                               String(sensorData.latitude, 6) + "," + 
                               String(sensorData.longitude, 6);
                sendSMS(message);
            }
            delay(50);  // Simple debounce
        }
    }
    
    // Initialize all hardware
    void initializeHardware() {
        // Initialize Serial communications
        Serial.begin(115200);
        gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
        
        // Initialize GPIO pins
        pinMode(BUTTON_PIN_GPS, INPUT_PULLUP);
        pinMode(BUTTON_PIN_ONOFF, INPUT_PULLUP);
        pinMode(BUZZ, OUTPUT);
        pinMode(WATER_SENSOR_PIN, INPUT);
        pinMode(TRIG_PIN1, OUTPUT);
        pinMode(ECHO_PIN1, INPUT);
        pinMode(TRIG_PIN2, OUTPUT);
        pinMode(ECHO_PIN2, INPUT);
        
        // Initialize outputs to known state
        digitalWrite(BUZZ, LOW);
        digitalWrite(TRIG_PIN1, LOW);
        digitalWrite(TRIG_PIN2, LOW);
    }
    
    // Initialize WiFi connection
    void initializeWiFi() {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        Serial.print("Connecting to WiFi");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected");
            Serial.println("IP address: " + WiFi.localIP().toString());
        } else {
            Serial.println("\nWiFi connection failed");
        }
    }

    // Initialize FreeRTOS resources
    void initializeFreeRTOS() {
        // Create queues
        sensorQueue = xQueueCreate(10, sizeof(SensorData));
        alertQueue = xQueueCreate(5, sizeof(Alert));
        
        // Create semaphores
        sensorMutex = xSemaphoreCreateMutex();
        webMutex = xSemaphoreCreateMutex();
        commMutex = xSemaphoreCreateMutex();
        objectDetectionMutex = xSemaphoreCreateMutex();
        
        // Create tasks
        xTaskCreatePinnedToCore(
            sensorTask,
            "SensorTask",
            SENSOR_STACK_SIZE,
            NULL,
            SENSOR_PRIORITY,
            &sensorTaskHandle,
            1  // Run on Core 1
        );
        
        xTaskCreatePinnedToCore(
            objectDetectionTask,
            "ObjectDetectionTask",
            STACK_SIZE,
            NULL,
            OBJECT_DETECT_PRIORITY,
            &objectDetectionTaskHandle,
            1  // Run on Core 1
        );
        
        xTaskCreatePinnedToCore(
            communicationTask,
            "CommTask",
            COMM_STACK_SIZE,
            NULL,
            COMM_PRIORITY,
            &communicationTaskHandle,
            0  // Run on Core 0
        );
        
        xTaskCreatePinnedToCore(
            webUpdateTask,
            "WebTask",
            WEB_STACK_SIZE,
            NULL,
            WEB_PRIORITY,
            &webUpdateTaskHandle,
            0  // Run on Core 0
        );
    }
    
    // Setup function
    void setup() {
        // Initialize hardware
        initializeHardware();
        
        // Initialize WiFi
        initializeWiFi();
        
        // Initialize SPIFFS
        if (!SPIFFS.begin(true)) {
            Serial.println("SPIFFS Mount Failed");
            return;
        }
        
        // Setup web server
        setupWebServer();
        
        // Initialize FreeRTOS tasks and resources
        initializeFreeRTOS();
        
        // Initialize sensor data structure
        memset(&sensorData, 0, sizeof(SensorData));
        sensorData.lastDetectedObject = "None";
        sensorData.timestamp = "--:--:--";
        
        Serial.println("System initialization complete");
    }
    
    // Main loop
    void loop() {
        // Handle button inputs
        handleButtons();
        
        // The main loop can be used for system-level monitoring
        // Most functionality is handled by FreeRTOS tasks
        
        // Optional: System monitoring
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost. Attempting to reconnect...");
            WiFi.reconnect();
        }
        
        // Add a small delay to prevent watchdog timer issues
        delay(100);
    }
