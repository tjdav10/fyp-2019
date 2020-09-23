import serial
import string
import sqlite3
ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = sqlite3.connect('example.db') # connecting to local db
while 1:
    if(ser.in_waiting >0):
        line = ser.readline()
        decode = line.decode('utf-8') # decoding into string
        x = decode.split() #splitting using space as delimitor
        print(x)
        if(x(0)=="B"):
            c = conn.cursor()
            c.execute("INSERT INTO batt_table VALUES (?,?)",x(1),x(2)) ## battery table
        else:
            c = conn.cursor()
            c.execute("INSERT INTO test_table VALUES (?,?)",x) ## can be changed to specify the fields
            conn.commit()