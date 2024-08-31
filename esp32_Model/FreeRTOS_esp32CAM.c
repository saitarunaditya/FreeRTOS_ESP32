#include <WiFi.h>
#include <esp_camera.h>
#include <Ultrasonic.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Replace with your Wi-Fi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Ultrasonic Sensor Pins
#define TRIGGER_PIN 12
#define ECHO_PIN 14

// IR Sensor Pin
#define IR_SENSOR_PIN 13

Ultrasonic ultrasonic(TRIGGER_PIN, ECHO_PIN);

// Function to start the camera web server
void startCameraServer() {
    // Camera configuration (replace with your camera settings)
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if(psramFound()){
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // Camera initialization
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    // Start the web server (replace with your own web server function)
    startCameraServer();
}

// Task 1: Camera Web Server
void taskCameraWebServer(void *pvParameters) {
    startCameraServer();
    while(true) {
        // Keep the task alive
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Task 2: Ultrasonic Sensor
void taskUltrasonic(void *pvParameters) {
    while(true) {
        long distance = ultrasonic.read();
        Serial.printf("Distance: %ld cm\n", distance);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Adjust delay as needed
    }
}

// Task 3: IR Sensor
void taskIRSensor(void *pvParameters) {
    pinMode(IR_SENSOR_PIN, INPUT);
    while(true) {
        int irValue = digitalRead(IR_SENSOR_PIN);
        Serial.printf("IR Sensor Value: %d\n", irValue);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Adjust delay as needed
    }
}

void setup() {
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(taskCameraWebServer, "Camera Web Server", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskUltrasonic, "Ultrasonic Sensor", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskIRSensor, "IR Sensor", 2048, NULL, 1, NULL, 0);
}

void loop() {
    // No need to add anything here, tasks are running independently
}
