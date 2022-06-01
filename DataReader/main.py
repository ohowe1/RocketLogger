#!/usr/bin/env python3
import csv
import struct
import sys
from bisect import bisect_left

gyro_multiplier = 8.75 * 0.017453293 / 1000
acceleration_multiplier = 0.061 * 9.80665  * 2 / 1000
magnetometer_multiplier = 1 / 6842
id_to_name = {
    0: ('GyroX', gyro_multiplier, 0),
    1: ('GyroY', gyro_multiplier, 0),
    2: ('GyroZ', gyro_multiplier, 0),
    3: ('AccelX', acceleration_multiplier, 0),
    4: ('AccelY', acceleration_multiplier, 0),
    5: ('AccelZ', acceleration_multiplier, 0),
    6: ('MagX', magnetometer_multiplier, 0),
    7: ('MagY', magnetometer_multiplier, 0),
    8: ('MagZ', magnetometer_multiplier, 0),
    9: ('Pressure', 1, 0),
    10: ('Temperature', 1 / 256, 25),
}

f = open('data.log', 'rb')
raw_data = f.read()

full_entries = (len(raw_data)) // 8
print(f'Found {full_entries} entries.')

# Read data from file
data = {}
timestamps = set()
for i in range(0, full_entries * 8, 8):
    entry = raw_data[i:i + 12]
    # Make 3 bit 4 bit
    value_bytes = entry[0:3] + bytes(1)
    value = struct.unpack('i', value_bytes)[0]
    # Negative
    if ((value >> 23) & 0x01) != 0:
        value = value - (1 << 24)

    id_ = entry[3]
    timestamp = struct.unpack('I', entry[4:8])[0]

    if id_ not in id_to_name:
        print(f'WARN: {id_} does not have a name. Using number as name')
        id_name = str(id_)
        multiplier = 1
        additive = 0
    else:
        id_name = id_to_name[id_][0]
        multiplier = id_to_name[id_][1]
        additive = id_to_name[id_][2]

    value *= multiplier
    print(f'Id: {id_name}, Timestamp: {timestamp}, Value: {value}')

    if id_name not in data:
        data[id_name] = {}
    data[id_name][timestamp] = value

    timestamps.add(timestamp)

data_names = sorted(list(data.keys()))

sorted_timestamps = {}
for name in data_names:
    sorted_timestamps[name] = sorted(data[name])

data_by_timestamp = {}
for time in sorted(timestamps):
    time_data = []
    for name in data_names:
        # timestamp with the best data for this time
        last_timestamp = sorted_timestamps[name][
            bisect_left(sorted_timestamps[name], time, hi=len(sorted_timestamps[name]) - 1)]
        time_data.append(data[name][last_timestamp])
    data_by_timestamp[time] = time_data

with open('data.csv', 'w') as csvfile:
    filewriter = csv.writer(csvfile)
    filewriter.writerow(['Timestamp (s)'] + data_names)
    for timestamp in data_by_timestamp:
        row = [timestamp / 1000] + data_by_timestamp[timestamp]
        filewriter.writerow(row)
