import serial
import pymysql
import string

ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = pymysql.connect(host='68.183.123.37',
                       user='tim',
                       password='monashtim123',
                       db='mysql',
                       port=8081,
                       cursorclass=pymysql.cursors.DictCursor)
while 1:
    i=1