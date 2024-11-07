![images-5](https://github.com/user-attachments/assets/41069ff2-ca7d-4424-9d00-f8e8ee1c56df)
![images](https://github.com/user-attachments/assets/e454002e-8678-4921-a191-3c745a0c5435)

# Task Scheduling and Communication in Real-Time Systems

## 1. RMS (Rate-Monotonic Scheduling)

**What It Is:**
RMS is a fixed-priority scheduling algorithm where tasks with shorter periods are assigned higher priorities. The idea is that tasks that need to be executed more frequently should be given higher priority to ensure timely execution.

**How It's Used in Your Project:**
- **Sensor Task:** The sensor task, which reads various sensors (water, ultrasonic, temperature), has the shortest period (100 ms), so it's assigned the highest priority using RMS. This ensures that the sensor data is collected as frequently as needed, which is crucial for real-time monitoring.
- **Object Detection Task:** The object detection task, with a slightly longer period (200 ms), gets the second-highest priority to ensure it runs frequently enough to detect objects and activate the buzzer in response.
- **Communication and Web Update Tasks:** These have longer periods (300 ms and 500 ms, respectively) and are assigned lower priorities to balance the system and ensure that high-priority tasks (like sensor data collection and object detection) are not delayed.

**Why It's Useful:**
RMS ensures that critical tasks like sensor data reading and object detection run without missing deadlines. This is especially important in real-time systems where timely data is essential for safety and accurate operation (e.g., water or object detection in your system).

---

## 2. EDF (Earliest Deadline First)

**What It Is:**
EDF is a dynamic priority scheduling algorithm where tasks with the nearest deadlines are given the highest priority. Unlike RMS, which uses fixed priorities, EDF adjusts the priority dynamically based on when a task's deadline is approaching.

**How It's Used in Your Project:**
In your communication task, the priority is adjusted dynamically based on the remaining time before the communication deadline. If the deadline for the task is near, the task's priority is increased to ensure that it completes before the deadline.

**Why It's Useful:**
EDF can handle tasks with varying time constraints more effectively, which can be crucial when tasks like communication need to be completed by specific times (e.g., sending alerts based on sensor data). This helps prevent missed deadlines, particularly in time-sensitive operations such as sending an SMS alert with location data when certain conditions (water detected, object proximity) occur.

---

## 3. PIP (Priority Inheritance Protocol)

**What It Is:**
PIP is a protocol that prevents priority inversion. Priority inversion occurs when a high-priority task is blocked by a lower-priority task holding a mutex. In PIP, the lower-priority task inherits the higher priority while holding the mutex, ensuring that it completes quickly and the higher-priority task can run as soon as possible.

**How It's Used in Your Project:**
In your project, when tasks need to access shared resources (like the `sensorMutex`, `commMutex`, or `objectDetectionMutex`), the PIP ensures that lower-priority tasks that hold these resources temporarily inherit the higher priority of the task waiting for them. This prevents high-priority tasks from being delayed by lower-priority ones that hold critical resources.

**Why It's Useful:**
PIP ensures that real-time tasks (such as sensor reading or communication) are not delayed due to resource contention. It enhances the responsiveness of critical tasks, which is important in a safety-critical application like your system that detects water or objects.

---

## 4. PCP (Priority Ceiling Protocol)

**What It Is:**
PCP is a protocol designed to prevent priority inversion by assigning a ceiling priority to each mutex. When a task locks a mutex, its priority is raised to the ceiling priority of that mutex, preventing other tasks with lower priorities from interfering.

**How It's Used in Your Project:**
Each mutex in your system (e.g., `sensorMutex`, `commMutex`, `objectDetectionMutex`) is assigned a ceiling priority that matches the priority of the task that most urgently needs access to the shared resource. For example, the `sensorMutex` has a ceiling priority equal to the highest priority task, ensuring that sensor-related tasks are executed without interruption.

**Why It's Useful:**
PCP ensures that tasks with higher urgency (e.g., sensor reading, communication) can access shared resources without being blocked by lower-priority tasks. This improves the efficiency and reliability of your system, ensuring that real-time tasks are handled without unnecessary delays.

---

## 5. UART Communication

**What It Is:**
UART (Universal Asynchronous Receiver/Transmitter) is a hardware communication protocol used for serial data transmission between devices. In your project, UART is used to interface with the GPS module to obtain real-time location data.

**How It's Used in Your Project:**
The GPS data is read via UART using the `HardwareSerial` object (`gpsSerial`), and the GPS data is processed by the `TinyGPS++` library to extract location information (latitude and longitude). This data is then used to send SMS alerts with location information when certain conditions (like water detection or object detection) are met.

**Why It's Useful:**
UART is a simple and efficient protocol for communication between the ESP32 and the GPS module. It allows real-time GPS data acquisition, which is essential for providing location-based alerts (such as sending a map link in an SMS when an object or water is detected).


![WhatsApp Image 2024-11-07 at 14 34 18_f142337a](https://github.com/user-attachments/assets/c9b81d46-4412-4a9e-8dfa-6c062291d889)

