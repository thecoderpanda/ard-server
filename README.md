# Arduino Sensor Data Server

A full-stack application for collecting, managing, and visualizing data from Arduino sensors. The system includes a Python server with both API endpoints and a web-based frontend CRUD interface.

## Setup

1. Clone this repository
2. Install dependencies:
   ```
   pip install -r requirements.txt
   ```
3. Run the server:
   ```
   python main.py
   ```
   
The server will run on `http://0.0.0.0:9090` and create a SQLite database file named `sensor_data.db`.

## Features

- Web-based CRUD interface for managing sensors and readings
- Interactive dashboard with charts
- Air Quality Index (AQI) categorization
- RESTful API for Arduino devices to send data
- SQLite database storage
- Automatic data collection from LoRa devices

## Automatic Data Collection

The system automatically collects air quality readings from Arduino devices:

1. **LoRa Sender**: Reads air quality values and transmits them via LoRa
2. **LoRa Receiver**: Receives data and automatically forwards it to the server API
3. **Server API**: Processes and stores the data, making it available through the web interface

To enable automatic data collection:

1. Update the `serverUrl` in the receiver code with your server's IP address
2. Upload the updated code to your ESP32 receiver
3. The receiver will automatically register with the server when it starts
4. Each time the receiver gets a new reading via LoRa, it will forward it to the server

No manual data entry is needed - just set up the hardware and the system will collect data automatically.

## Web Interface

The application provides a full web interface accessible at `http://0.0.0.0:9090`:

- **Dashboard**: Overview of all sensors with their latest readings
- **Sensors**: Manage (add/edit/delete) sensors
- **Readings**: View, add, edit, and delete readings for each sensor
- **Analytics**: Interactive charts showing sensor data over time

## API Endpoints

### Register a Sensor
```
POST /api/sensor/register
```
Request body:
```json
{
  "name": "air_quality_sensor_1",
  "description": "Air quality sensor in living room"
}
```
Response:
```json
{
  "status": "success",
  "sensor_id": 1
}
```

### Send Sensor Data
```
POST /api/sensor/data
```
Request body:
```json
{
  "sensor_id": 1,
  "value": 125,
  "timestamp": "2023-05-01T15:30:00"  // Optional, defaults to current time
}
```

### LoRa Device Data Endpoint
```
POST /api/lora/data
```
Request body:
```json
{
  "sensor_id": 1,  // Optional, will use default sensor if not provided
  "value": 125,
  "timestamp": "2023-05-01T15:30:00"  // Optional, defaults to current time
}
```

### Get Sensor Data
```
GET /api/sensor/data/{sensor_id}?limit=100
```
The `limit` parameter is optional and defaults to 100 readings.

### List All Sensors
```
GET /api/sensors
```

## Arduino Implementation

The project includes examples of Arduino code for both sender and receiver using LoRa communication:

1. **Sender (`arduino_sensor_example/sender.ino`)**: Reads from an MQ135 air quality sensor and sends data via LoRa
2. **Receiver (`arduino_sensor_example/reciever.ino`)**: Receives data via LoRa, connects to WiFi, and:
   - Forwards data to the main server API
   - Serves a simple local web interface

## Air Quality Index (AQI) Categories

The application categorizes air quality readings into the following categories:

- **Good (0-50)**: Air quality is satisfactory, poses little or no risk
- **Moderate (51-100)**: Acceptable air quality, but may cause concern for very sensitive people
- **Unhealthy for Sensitive Groups (101-150)**: May affect sensitive groups
- **Unhealthy (151-200)**: Everyone may begin to experience health effects
- **Very Unhealthy (201-300)**: Health alert; everyone may experience more serious health effects
- **Hazardous (301+)**: Health warning of emergency conditions

## License

MIT 