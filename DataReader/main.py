#!/usr/bin/env python3
import csv
import struct
from bisect import bisect_left

id_to_name = {
    1: 'GyroX (degrees)',
    2: 'Air Pressure (psi)'
}

f = open('DATA.BINLOG', 'rb')
raw_data = f.read()

full_entries = (len(raw_data)) // 12
print(f'Found {full_entries} entries.')

# Read data from file
# TODO: Support the beginning properly (8 0xFFs)
data = {}
timestamps = set()
for i in range(0, full_entries * 12, 12):
    entry = raw_data[i:i + 12]
    id_ = struct.unpack('I', entry[0:4])[0]  # 4 bit integer
    timestamp = struct.unpack('I', entry[4:8])[0]  # 4 bit integer
    value = struct.unpack('f', entry[8:12])[0]  # 4 bit floating point number

    if id_ not in id_to_name:
        print(f'WARN: {id_} does not have a name. Using number as name')
        id_name = str(id_)
    else:
        id_name = id_to_name[id_]

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
