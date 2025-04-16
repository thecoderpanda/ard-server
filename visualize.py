import sqlite3
import matplotlib.pyplot as plt
import pandas as pd
import datetime
import argparse

def get_sensor_data(sensor_id, limit=100):
    """Fetch sensor data from the database for visualization"""
    conn = sqlite3.connect('sensor_data.db')
    
    query = f"""
    SELECT r.value, r.timestamp, s.name 
    FROM readings r
    JOIN sensors s ON r.sensor_id = s.id
    WHERE r.sensor_id = {sensor_id}
    ORDER BY r.timestamp ASC
    LIMIT {limit}
    """
    
    df = pd.read_sql_query(query, conn)
    conn.close()
    
    # Convert timestamp string to datetime
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    return df

def list_sensors():
    """List all sensors in the database"""
    conn = sqlite3.connect('sensor_data.db')
    cursor = conn.cursor()
    
    cursor.execute("SELECT id, name, description FROM sensors")
    sensors = cursor.fetchall()
    
    conn.close()
    
    print("Available sensors:")
    for sensor in sensors:
        print(f"ID: {sensor[0]}, Name: {sensor[1]}, Description: {sensor[2]}")

def plot_sensor_data(sensor_id, limit=100):
    """Plot sensor data as a time series"""
    df = get_sensor_data(sensor_id, limit)
    
    if df.empty:
        print(f"No data found for sensor ID {sensor_id}")
        return
    
    sensor_name = df['name'].iloc[0]
    
    plt.figure(figsize=(12, 6))
    plt.plot(df['timestamp'], df['value'], marker='o', linestyle='-')
    plt.title(f'Sensor Data: {sensor_name} (ID: {sensor_id})')
    plt.xlabel('Time')
    plt.ylabel('Value')
    plt.grid(True)
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    # Save plot
    filename = f"sensor_{sensor_id}_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
    plt.savefig(filename)
    print(f"Plot saved as {filename}")
    
    # Show plot
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Visualize sensor data')
    parser.add_argument('--list', action='store_true', help='List all available sensors')
    parser.add_argument('--sensor', type=int, help='Sensor ID to visualize')
    parser.add_argument('--limit', type=int, default=100, help='Maximum number of readings to fetch')
    
    args = parser.parse_args()
    
    if args.list:
        list_sensors()
    elif args.sensor is not None:
        plot_sensor_data(args.sensor, args.limit)
    else:
        parser.print_help() 