#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER  // Has PSRAM

// Pin definition for AI Thinker Model
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Thulasi";
const char *password = "starman142";

// Ultrasonic Sensor pins
#define TRIG_PIN 12
#define ECHO_PIN 13

// IR Sensor pin
#define IR_PIN 14

// Built-in LED pin
#define LED_PIN 4

// Task handles
TaskHandle_t cameraTaskHandle = NULL;
TaskHandle_t ultrasonicTaskHandle = NULL;
TaskHandle_t irTaskHandle = NULL;

// Global server handle
httpd_handle_t server = NULL;

void startCameraServer();
esp_err_t index_handler(httpd_req_t *req);
void setupLedFlash(int pin);

// Task 1: Camera Web Server
void cameraTask(void *pvParameters) {
  startCameraServer();
  while (true) {
    delay(1000);
  }
}

// Task 2: Ultrasonic Sensor
void ultrasonicTask(void *pvParameters) {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  while (true) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = duration * 0.034 / 2;
    
    Serial.print("Ultrasonic Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    
    delay(1000);
  }
}

// Task 3: IR Sensor
void irTask(void *pvParameters) {
  pinMode(IR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  while (true) {
    int irValue = digitalRead(IR_PIN);
    Serial.print("IR Sensor Value: ");
    Serial.println(irValue);
    
    if (irValue == HIGH) {
      digitalWrite(LED_PIN, HIGH);  // Turn on the built-in LED
    } else {
      digitalWrite(LED_PIN, LOW);   // Turn off the built-in LED
    }
    
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.frame_size = FRAMESIZE_UXGA;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  s->set_framesize(s, FRAMESIZE_QVGA);

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.println("Camera initialized successfully");

  // Start the camera web server by default
  Serial.println("Starting Camera Server...");
  xTaskCreatePinnedToCore(cameraTask, "Camera Task", 10000, NULL, 1, &cameraTaskHandle, 1);
}

void loop() {
  if (Serial.available()) {
    int command = Serial.parseInt();
    
    switch (command) {
      case 1:
        if (cameraTaskHandle == NULL) {
          Serial.println("Starting Camera Server...");
          xTaskCreatePinnedToCore(cameraTask, "Camera Task", 10000, NULL, 1, &cameraTaskHandle, 1);
        }
        break;
        
      case 2:
        if (ultrasonicTaskHandle == NULL) {
          Serial.println("Stopping Camera Server...");
          if (cameraTaskHandle != NULL) {
            vTaskDelete(cameraTaskHandle);
            cameraTaskHandle = NULL;
          }
          Serial.println("Starting Ultrasonic Task...");
          xTaskCreatePinnedToCore(ultrasonicTask, "Ultrasonic Task", 10000, NULL, 1, &ultrasonicTaskHandle, 1);
        }
        break;
        
      case 3:
        if (irTaskHandle == NULL) {
          Serial.println("Stopping Camera Server...");
          if (cameraTaskHandle != NULL) {
            vTaskDelete(cameraTaskHandle);
            cameraTaskHandle = NULL;
          }
          Serial.println("Starting IR Task...");
          xTaskCreatePinnedToCore(irTask, "IR Task", 10000, NULL, 1, &irTaskHandle, 1);
        }
        break;
        
      default:
        Serial.println("Invalid command. Enter 1 for Camera, 2 for Ultrasonic, or 3 for IR.");
        break;
    }
  }
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
  };

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &index_uri);
  }

  Serial.print("Camera server started at: ");
  Serial.println(WiFi.localIP());
}

esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, "<html><body><h1>ESP32-CAM Web Server</h1></body></html>", -1);
}

void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}
