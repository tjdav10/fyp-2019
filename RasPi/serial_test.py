import serial
import string
ser = serial.Serial('/dev/ttyACM0', 115200) # ACM0 is top left USB port
while 1:
    if(ser.in_waiting >0):
        line = ser.readline()
        decode = line.decode('utf-8') # decoding into string
        print(decode)