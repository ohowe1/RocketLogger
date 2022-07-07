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
    11: ('Exterior Temperature', 100, 0),
}

ENTRY_SIZE = 4

f = open('data.log', 'rb')
raw_data = f.read()

amount_of_bytes = len(raw_data)
full_entries = amount_of_bytes // ENTRY_SIZE
print(f'Found {full_entries} entries.')

# Read data from file
data = {}
timestamps = set()
awaiting_timestamp_buffer = []
i = 0
while i < amount_of_bytes:
    if i >> 6 == 0b11:
        if i & 0b00111111 == len(awaiting_timestamp_buffer):
            # Validation byte
            i += 1
            timestamp_bytes = raw_data[i + 1]
            timestamp = struct.unpack('i', timestamp_bytes)[0]
            timestamps.add(timestamp)
            for awaiting in awaiting_timestamp_buffer:
                id_name = awaiting[0]
                value = awaiting[1]
                if id_name not in data:
                    data[id_name] = {}
                    data[id_name][timestamp] = value
                print(f'Id: {id_name}, Timestamp: {timestamp}, Value: {value}')
            i += 4
        else:
            print(f'ERR: Invalid timestamp at byte {i}')
            sys.exit()
    else:
        if i + ENTRY_SIZE <= amount_of_bytes:
            print('WARN: Could not complete entry! Continuing with current entries')
            break
        else:
            entry = raw_data[i:i + ENTRY_SIZE]
            # Make 3 bit 4 bit
            value_bytes = entry[0:3] + bytes(1)
            value = struct.unpack('i', value_bytes)[0]
            # Negative
            if ((value >> 23) & 0x01) != 0:
                value = value - (1 << 24)
            id_ = entry[3]

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
            value += additive
            awaiting_timestamp_buffer.append((id_name, value))

            i += ENTRY_SIZE


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
