#!/usr/bin/env python3
import csv
import struct
import sys
from bisect import bisect_left

gyro_multiplier = 8.75 * 0.017453293 / 1000
acceleration_multiplier = 0.122 * 9.80665 * 2 / 1000
magnetometer_multiplier = 1 / 6842
id_to_name = {
    0: ('GyroX (rad/s)', gyro_multiplier, 0),
    1: ('GyroY (rad/s)', gyro_multiplier, 0),
    2: ('GyroZ (rad/s)', gyro_multiplier, 0),
    3: ('AccelX (m/s^2)', acceleration_multiplier, 0),
    4: ('AccelY (m/s^2)', acceleration_multiplier, 0),
    5: ('AccelZ (m/s^2)', acceleration_multiplier, 0),
    6: ('MagX (G)', magnetometer_multiplier, 0),
    7: ('MagY (G)', magnetometer_multiplier, 0),
    8: ('MagZ (G)', magnetometer_multiplier, 0),
    9: ('Pressure (Pa)', 1, 0),
    10: ('Temperature (C)', 1 / 256, 25),
    11: ('Exterior Temperature (C)', 1 / 100, 0),
}

ENTRY_SIZE = 4

f = open('data.log', 'rb')
raw_data = f.read()

amount_of_bytes = len(raw_data)
print(f'Found {amount_of_bytes} bytes')

# Read data from file
data = {}
timestamps = set()
awaiting_timestamp_buffer = []
i = 0
has_first_timestamp = False
first_timestamp = 0
while i < amount_of_bytes:
    if raw_data[i] == 0xff:
        if i + 8 <= amount_of_bytes and struct.unpack('I', raw_data[i:i+3] + bytes(1))[0] == 0xffffff:
            if (raw_data[i + 3]) == len(awaiting_timestamp_buffer):
                # Validation bytes
                i += 4
                timestamp_bytes = raw_data[i:i+4]
                timestamp = struct.unpack('I', timestamp_bytes)[0]
                if not has_first_timestamp:
                    first_timestamp = timestamp
                    timestamp = 0
                    has_first_timestamp = True
                else:
                    timestamp -= first_timestamp
                timestamps.add(timestamp)
                for awaiting in awaiting_timestamp_buffer:
                    id_name = awaiting[0]
                    value = awaiting[1]
                    if id_name not in data:
                        data[id_name] = {}
                    data[id_name][timestamp] = value
                    print(f'Id: {id_name}, Timestamp: {timestamp}, Value: {value}')
                awaiting_timestamp_buffer.clear()
                i += 4
            else:
                print(f'ERR: Invalid timestamp at byte {i}')
                print(raw_data[i + 3])
                print(len(awaiting_timestamp_buffer))
                sys.exit()
        else:
            print(struct.unpack('I', raw_data[i:i+3] + bytes(1)))
            print(f'ERR: Invalid timestamp at byte {i}')
            sys.exit()
    else:
        if i + ENTRY_SIZE >= amount_of_bytes:
            print('WARN: Could not complete entry! Continuing with current entries')
            break
        else:
            entry = raw_data[i:i + ENTRY_SIZE]
            # Make 3 bit 4 bit
            value_bytes = entry[1:4] + bytes(1)
            value = struct.unpack('i', value_bytes)[0]
            # Negative
            if ((value >> 23) & 0x01) != 0:
                value = value - (1 << 24)
            id_ = entry[0]

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
