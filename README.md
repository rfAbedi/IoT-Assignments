# IoT-Assignments

Assignments for **IoT**

Instructor: *Dr. attarzadeh*

University: Shahid Beheshti University (SBU)

---

## Technologies & Tools

| Layer | Tools & Technologies |
|-------|----------------------|
| **Edge / Device Layer** | C++, ESP32, PlatformIO, Wokwi |
| **Communication Layer** | MQTT (Eclipse Mosquitto), OPC UA Server |
| **Processing / Middleware** | Node-RED, Apache Airflow |
| **Data Layer** | InfluxDB, Telegraf |
| **Visualization / Dashboard** | Grafana, ThingsBoard |
| **Deployment & Simulation** | Docker, Wokwi |

---

## How to Run

### Deployment

  ```bash
   docker-compose up -d
  ```

### Visualization and Dashboards

+ **Node-RED**: subscribes and visualize data.
  - Access via http://localhost:1880.
+ **Airflow**: triggers periodic data cleanups
  - Access via http://localhost:8080
+ **Grafana**: visualize real-time sensor data from InfluxDB.
  - Access via http://localhost:3000 (default user/pass: admin/admin).
+ **ThingsBoard**: optional IoT platform for device management and dashboards.
  - Access via http://localhost:8080 (default user/pass: tenant@thingsboard.org/tenant).



