# How to differentiate between my log and original? mine has a space as the 5th
# char
# make a function to send time
import serial
import string
import sqlite3
import time
ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = sqlite3.connect('example.db') # connecting to local db

while 1:
    if(ser.in_waiting >0):
        line = ser.readline()
        decode = line.decode('utf-8') # decoding into string
<<<<<<< HEAD
        if decode[4]==" ":
            x = decode.split() #splitting using space as delimitor
            print(x)
            if x[0]=="B": # battery reading
                v_meas = float(x[2])
                v_max = float(x[3])
                v_min = float(x[4])
                b_level = int((v_meas-v_min)/(v_max-v_min)*100) # computing battery as %
                c = conn.cursor()
                for_upload = (x[1], b_level)
                c.execute("INSERT INTO battery VALUES (?,?)", for_upload) ## can be changed to specify the fields
                conn.commit()
                received_time()
            else:
                c = conn.cursor()
                for_upload = (x[0], x[1], x[2], x[3])
                c.execute("INSERT INTO interaction_log VALUES (?,?,?,?)", for_upload)
                conn.commit()
                received_time()
=======
        x = decode.split() #splitting using space as delimitor
        print(x)
        if(x(0)=="B"):
            c = conn.cursor()
            c.execute("INSERT INTO batt_table VALUES (?,?)",x(1),x(2)) ## battery table
        else:
            c = conn.cursor()
            c.execute("INSERT INTO test_table VALUES (?,?)",x) ## can be changed to specify the fields
            conn.commit()
>>>>>>> 6e810468ec67ed80f873acfcc7bc0a067c9eb058
