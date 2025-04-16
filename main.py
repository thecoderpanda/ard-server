from flask import Flask, request, jsonify, render_template, redirect, url_for, flash
import sqlite3
import os
import datetime

app = Flask(__name__)
app.secret_key = os.urandom(24)  # For flash messages

# Initialize database
def init_db():
    conn = sqlite3.connect('sensor_data.db')
    c = conn.cursor()
    
    # Check if database exists and has the old schema
    try:
        c.execute('SELECT * FROM readings LIMIT 1')
        # If we get here, table exists. Let's check the columns
        columns = [description[0] for description in c.description]
        
        # If we don't have the new columns, alter the table
        if 'aqi_value' not in columns:
            print("Upgrading database schema...")
            # Rename old table
            c.execute('ALTER TABLE readings RENAME TO readings_old')
            
            # Create new table with updated schema
            c.execute('''
                CREATE TABLE readings (
                    id INTEGER PRIMARY KEY,
                    sensor_id INTEGER,
                    aqi_value REAL,
                    co2_ppm REAL,
                    aqi_category TEXT,
                    timestamp DATETIME,
                    FOREIGN KEY (sensor_id) REFERENCES sensors (id)
                )
            ''')
            
            # Migrate old data
            c.execute('''
                INSERT INTO readings (id, sensor_id, aqi_value, timestamp)
                SELECT id, sensor_id, value, timestamp FROM readings_old
            ''')
            
            # Drop old table
            c.execute('DROP TABLE readings_old')
            
            conn.commit()
            print("Database schema upgraded successfully")
    except sqlite3.OperationalError:
        # Table doesn't exist, create from scratch
        c.execute('''
            CREATE TABLE IF NOT EXISTS sensors (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT
            )
        ''')
        c.execute('''
            CREATE TABLE IF NOT EXISTS readings (
                id INTEGER PRIMARY KEY,
                sensor_id INTEGER,
                aqi_value REAL,
                co2_ppm REAL,
                aqi_category TEXT,
                timestamp DATETIME,
                FOREIGN KEY (sensor_id) REFERENCES sensors (id)
            )
        ''')
        
        # Add a default AQI sensor if not exists
        c.execute('INSERT OR IGNORE INTO sensors (name, description) VALUES (?, ?)', 
                  ('Default AQI Sensor', 'Automatically created AQI sensor'))
        
        conn.commit()
        print("Database initialized with new schema")
    
    conn.close()

def get_db_connection():
    conn = sqlite3.connect('sensor_data.db')
    conn.row_factory = sqlite3.Row
    return conn

def get_aqi_category(aqi):
    try:
        aqi_value = float(aqi)
        if aqi_value <= 50:
            return "Good", "good"
        elif aqi_value <= 100:
            return "Moderate", "moderate"
        elif aqi_value <= 150:
            return "Unhealthy for Sensitive Groups", "unhealthy-sensitive"
        elif aqi_value <= 200:
            return "Unhealthy", "unhealthy"
        elif aqi_value <= 300:
            return "Very Unhealthy", "very-unhealthy"
        else:
            return "Hazardous", "hazardous"
    except:
        return "Unknown", ""

# Frontend routes
@app.route('/')
def index():
    conn = get_db_connection()
    # Get latest reading from each sensor
    latest_readings = conn.execute('''
        SELECT s.id, s.name, s.description, 
               r.aqi_value as value, r.co2_ppm, r.aqi_category, r.timestamp
        FROM sensors s
        LEFT JOIN (
            SELECT sensor_id, aqi_value, co2_ppm, aqi_category, timestamp
            FROM readings r1
            WHERE timestamp = (
                SELECT MAX(timestamp) FROM readings r2 WHERE r2.sensor_id = r1.sensor_id
            )
        ) r ON s.id = r.sensor_id
        ORDER BY s.name
    ''').fetchall()
    
    # Process data for display
    sensors_data = []
    for reading in latest_readings:
        category, class_name = get_aqi_category(reading['value'] if reading['value'] else 0)
        sensors_data.append({
            'id': reading['id'],
            'name': reading['name'],
            'description': reading['description'],
            'value': reading['value'],
            'co2_ppm': reading['co2_ppm'] if 'co2_ppm' in reading.keys() else None,
            'timestamp': reading['timestamp'],
            'category': reading['aqi_category'] if reading['aqi_category'] else category,
            'class_name': class_name
        })
    
    conn.close()
    return render_template('index.html', sensors=sensors_data)

@app.route('/sensors')
def list_sensors():
    conn = get_db_connection()
    sensors = conn.execute('SELECT id, name, description FROM sensors').fetchall()
    conn.close()
    return render_template('sensors.html', sensors=sensors)

@app.route('/sensors/add', methods=['GET', 'POST'])
def add_sensor():
    if request.method == 'POST':
        name = request.form['name']
        description = request.form['description']
        
        if not name:
            flash('Sensor name is required!')
            return redirect(url_for('add_sensor'))
        
        conn = get_db_connection()
        conn.execute('INSERT INTO sensors (name, description) VALUES (?, ?)', 
                    (name, description))
        conn.commit()
        conn.close()
        
        flash(f'Sensor "{name}" was added successfully!')
        return redirect(url_for('list_sensors'))
    
    return render_template('sensor_form.html', sensor=None)

@app.route('/sensors/<int:id>/edit', methods=['GET', 'POST'])
def edit_sensor(id):
    conn = get_db_connection()
    sensor = conn.execute('SELECT id, name, description FROM sensors WHERE id = ?', (id,)).fetchone()
    
    if sensor is None:
        conn.close()
        flash('Sensor not found!')
        return redirect(url_for('list_sensors'))
    
    if request.method == 'POST':
        name = request.form['name']
        description = request.form['description']
        
        if not name:
            flash('Sensor name is required!')
            return redirect(url_for('edit_sensor', id=id))
        
        conn.execute('UPDATE sensors SET name = ?, description = ? WHERE id = ?', 
                    (name, description, id))
        conn.commit()
        conn.close()
        
        flash(f'Sensor "{name}" was updated successfully!')
        return redirect(url_for('list_sensors'))
    
    conn.close()
    return render_template('sensor_form.html', sensor=sensor)

@app.route('/sensors/<int:id>/delete', methods=['POST'])
def delete_sensor(id):
    conn = get_db_connection()
    sensor = conn.execute('SELECT name FROM sensors WHERE id = ?', (id,)).fetchone()
    
    if sensor is None:
        conn.close()
        flash('Sensor not found!')
        return redirect(url_for('list_sensors'))
    
    # First delete all readings for this sensor
    conn.execute('DELETE FROM readings WHERE sensor_id = ?', (id,))
    # Then delete the sensor
    conn.execute('DELETE FROM sensors WHERE id = ?', (id,))
    conn.commit()
    conn.close()
    
    flash(f'Sensor "{sensor["name"]}" and all its readings were deleted!')
    return redirect(url_for('list_sensors'))

@app.route('/sensors/<int:id>/readings')
def sensor_readings(id):
    conn = get_db_connection()
    sensor = conn.execute('SELECT id, name, description FROM sensors WHERE id = ?', (id,)).fetchone()
    
    if sensor is None:
        conn.close()
        flash('Sensor not found!')
        return redirect(url_for('list_sensors'))
    
    readings = conn.execute('''
        SELECT id, aqi_value as value, co2_ppm, aqi_category, timestamp
        FROM readings
        WHERE sensor_id = ?
        ORDER BY timestamp DESC
        LIMIT 100
    ''', (id,)).fetchall()
    
    # Process data to include AQI categories
    processed_readings = []
    for reading in readings:
        category, class_name = get_aqi_category(reading['value'])
        processed_readings.append({
            'id': reading['id'],
            'value': reading['value'],
            'co2_ppm': reading['co2_ppm'] if 'co2_ppm' in reading.keys() else None,
            'timestamp': reading['timestamp'],
            'category': reading['aqi_category'] if reading['aqi_category'] else category,
            'class_name': class_name
        })
    
    conn.close()
    return render_template('readings.html', sensor=sensor, readings=processed_readings)

@app.route('/readings/add', methods=['GET', 'POST'])
def add_reading():
    conn = get_db_connection()
    sensors = conn.execute('SELECT id, name FROM sensors').fetchall()
    
    if not sensors:
        conn.close()
        flash('You need to add a sensor first!')
        return redirect(url_for('add_sensor'))
    
    if request.method == 'POST':
        sensor_id = request.form['sensor_id']
        value = request.form['value']
        timestamp = request.form.get('timestamp', datetime.datetime.now().isoformat())
        
        if not value:
            flash('Value is required!')
            return redirect(url_for('add_reading'))
        
        conn.execute('INSERT INTO readings (sensor_id, value, timestamp) VALUES (?, ?, ?)', 
                    (sensor_id, value, timestamp))
        conn.commit()
        conn.close()
        
        flash('Reading was added successfully!')
        return redirect(url_for('sensor_readings', id=sensor_id))
    
    conn.close()
    return render_template('reading_form.html', sensors=sensors, reading=None)

@app.route('/readings/<int:id>/edit', methods=['GET', 'POST'])
def edit_reading(id):
    conn = get_db_connection()
    reading = conn.execute('''
        SELECT r.id, r.sensor_id, r.value, r.timestamp, s.name as sensor_name
        FROM readings r
        JOIN sensors s ON r.sensor_id = s.id
        WHERE r.id = ?
    ''', (id,)).fetchone()
    
    if reading is None:
        conn.close()
        flash('Reading not found!')
        return redirect(url_for('index'))
    
    sensors = conn.execute('SELECT id, name FROM sensors').fetchall()
    
    if request.method == 'POST':
        sensor_id = request.form['sensor_id']
        value = request.form['value']
        timestamp = request.form['timestamp']
        
        if not value:
            flash('Value is required!')
            return redirect(url_for('edit_reading', id=id))
        
        conn.execute('UPDATE readings SET sensor_id = ?, value = ?, timestamp = ? WHERE id = ?', 
                    (sensor_id, value, timestamp, id))
        conn.commit()
        conn.close()
        
        flash('Reading was updated successfully!')
        return redirect(url_for('sensor_readings', id=sensor_id))
    
    conn.close()
    return render_template('reading_form.html', sensors=sensors, reading=reading)

@app.route('/readings/<int:id>/delete', methods=['POST'])
def delete_reading(id):
    conn = get_db_connection()
    reading = conn.execute('SELECT sensor_id FROM readings WHERE id = ?', (id,)).fetchone()
    
    if reading is None:
        conn.close()
        flash('Reading not found!')
        return redirect(url_for('index'))
    
    sensor_id = reading['sensor_id']
    
    conn.execute('DELETE FROM readings WHERE id = ?', (id,))
    conn.commit()
    conn.close()
    
    flash('Reading was deleted!')
    return redirect(url_for('sensor_readings', id=sensor_id))

@app.route('/dashboard')
def dashboard():
    return render_template('dashboard.html')

@app.route('/dashboard/data')
def dashboard_data():
    conn = get_db_connection()
    
    # Get data for all sensors in the last 24 hours
    readings = conn.execute('''
        SELECT r.sensor_id, s.name, r.value, r.timestamp
        FROM readings r
        JOIN sensors s ON r.sensor_id = s.id
        WHERE r.timestamp >= datetime('now', '-1 day')
        ORDER BY r.timestamp
    ''').fetchall()
    
    # Format data for charts
    sensors = {}
    for reading in readings:
        sensor_id = reading['sensor_id']
        if sensor_id not in sensors:
            sensors[sensor_id] = {
                'name': reading['name'],
                'values': [],
                'timestamps': []
            }
        sensors[sensor_id]['values'].append(float(reading['value']))
        sensors[sensor_id]['timestamps'].append(reading['timestamp'])
    
    conn.close()
    return jsonify(sensors)

# API routes (original)
@app.route('/api/sensor/register', methods=['POST'])
def register_sensor():
    data = request.get_json()
    
    if not data or 'name' not in data:
        return jsonify({"error": "Sensor name is required"}), 400
    
    name = data['name']
    description = data.get('description', '')
    
    conn = get_db_connection()
    
    # Check if sensor already exists
    existing = conn.execute('SELECT id FROM sensors WHERE name = ?', (name,)).fetchone()
    
    if existing:
        sensor_id = existing['id']
    else:
        cursor = conn.execute('INSERT INTO sensors (name, description) VALUES (?, ?)', 
                 (name, description))
        sensor_id = cursor.lastrowid
    
    conn.commit()
    conn.close()
    
    return jsonify({"status": "success", "sensor_id": sensor_id}), 201

@app.route('/api/sensor/data', methods=['POST'])
def add_sensor_data():
    data = request.get_json()
    
    if not data or 'sensor_id' not in data or 'value' not in data:
        return jsonify({"error": "Sensor ID and value are required"}), 400
    
    sensor_id = data['sensor_id']
    value = data['value']
    timestamp = data.get('timestamp', datetime.datetime.now().isoformat())
    
    conn = get_db_connection()
    
    # Check if sensor exists
    sensor = conn.execute('SELECT id FROM sensors WHERE id = ?', (sensor_id,)).fetchone()
    if not sensor:
        conn.close()
        return jsonify({"error": "Sensor not found"}), 404
    
    conn.execute('INSERT INTO readings (sensor_id, value, timestamp) VALUES (?, ?, ?)', 
             (sensor_id, value, timestamp))
    
    conn.commit()
    conn.close()
    
    return jsonify({"status": "success"}), 201

@app.route('/api/sensor/data/<int:sensor_id>', methods=['GET'])
def get_sensor_data(sensor_id):
    limit = request.args.get('limit', 100, type=int)
    
    conn = get_db_connection()
    
    readings = conn.execute('''
        SELECT r.id, r.value, r.timestamp, s.name 
        FROM readings r
        JOIN sensors s ON r.sensor_id = s.id
        WHERE r.sensor_id = ?
        ORDER BY r.timestamp DESC
        LIMIT ?
    ''', (sensor_id, limit)).fetchall()
    
    result = [dict(reading) for reading in readings]
    conn.close()
    
    return jsonify(result)

@app.route('/api/sensors', methods=['GET'])
def get_all_sensors():
    conn = get_db_connection()
    
    sensors = conn.execute('SELECT id, name, description FROM sensors').fetchall()
    result = [dict(sensor) for sensor in sensors]
    conn.close()
    
    return jsonify(result)

# Special API route for LoRa devices
@app.route('/api/lora/data', methods=['POST'])
def add_lora_data():
    data = request.get_json()
    
    if not data or 'value' not in data:
        return jsonify({"error": "Value is required"}), 400
    
    # Parse the value string to extract CO2 and AQI
    value_str = str(data['value'])
    co2_ppm = None
    aqi_value = None
    aqi_category = None
    
    # Handle both string format and direct numeric format
    if isinstance(data['value'], (int, float)):
        # If direct numeric value, treat as AQI
        aqi_value = float(data['value'])
    else:
        # Try to parse the string format
        try:
            if "CO2:" in value_str and "AQI:" in value_str:
                # Parse CO2
                co2_start = value_str.find("CO2:") + 4
                co2_end = value_str.find("ppm")
                if co2_start > 3 and co2_end > co2_start:
                    co2_ppm = float(value_str[co2_start:co2_end].strip())
                
                # Parse AQI
                aqi_start = value_str.find("AQI:") + 4
                aqi_end = value_str.find(",", aqi_start)
                if aqi_start > 3:
                    if aqi_end == -1:
                        aqi_value = float(value_str[aqi_start:].strip())
                    else:
                        aqi_value = float(value_str[aqi_start:aqi_end].strip())
                
                # Parse Zone if available
                if "Zone:" in value_str:
                    zone_start = value_str.find("Zone:") + 5
                    aqi_category = value_str[zone_start:].strip()
            else:
                # Treat as direct AQI value
                aqi_value = float(value_str)
        except (ValueError, IndexError) as e:
            return jsonify({"error": f"Failed to parse value: {str(e)}"}), 400
    
    timestamp = data.get('timestamp', datetime.datetime.now().isoformat())
    
    # Either use provided sensor_id or get default sensor
    if 'sensor_id' in data and data['sensor_id']:
        sensor_id = data['sensor_id']
    else:
        conn = get_db_connection()
        sensor = conn.execute('SELECT id FROM sensors WHERE name = ?', 
                            ('Default AQI Sensor',)).fetchone()
        
        if sensor:
            sensor_id = sensor['id']
        else:
            cursor = conn.execute('INSERT INTO sensors (name, description) VALUES (?, ?)', 
                     ('Default AQI Sensor', 'Automatically created AQI sensor'))
            sensor_id = cursor.lastrowid
            conn.commit()
        conn.close()
    
    # Store the reading
    conn = get_db_connection()
    try:
        conn.execute('''
            INSERT INTO readings (sensor_id, aqi_value, co2_ppm, aqi_category, timestamp) 
            VALUES (?, ?, ?, ?, ?)
        ''', (sensor_id, aqi_value, co2_ppm, aqi_category, timestamp))
        conn.commit()
        success = True
    except sqlite3.OperationalError:
        # Fallback to old schema if new schema fails
        conn.execute('INSERT INTO readings (sensor_id, value, timestamp) VALUES (?, ?, ?)', 
                    (sensor_id, aqi_value, timestamp))
        conn.commit()
        success = True
    finally:
        conn.close()
    
    return jsonify({
        "status": "success",
        "stored_data": {
            "aqi_value": aqi_value,
            "co2_ppm": co2_ppm,
            "aqi_category": aqi_category,
            "timestamp": timestamp
        }
    }), 201

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=9090, debug=True)
