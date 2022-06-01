import glob
import serial

ports = glob.glob('/dev/tty.*')
print('Ports:')
for i in range(len(ports)):
    print(f'{i}: {ports[i]}')

port_num = int(input('What port would you like to use? '))
port = ports[port_num]

print('Send data when ready... Use Ctrl+C when data has been sent as indicated on the device')
clue = serial.Serial(port=port, baudrate=9600)
write_file = open('data.log', 'wb')
received = 0
try:
    while True:
        write_file.write(clue.read(1))
        received += 1
except KeyboardInterrupt:
    print(f'\nReceived {received} bytes')
    write_file.close()
