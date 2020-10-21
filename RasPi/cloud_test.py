import serial
import pymysql.cursors
import string
from datetime import date, datetime

ser = serial.Serial('/dev/ttyACM1', 9600) # opening serial port, ACM0 is top left USB port
conn = pymysql.connect(host='128.199.68.151',
                       user='tim',
                       password='monashtim123',
                       db='hand_hygiene',
                       port=3306,
                       cursorclass=pymysql.cursors.DictCursor)
print("connected to cloud")
while 1:
    today_date = str(date.today())
    test=0
#     print(today_date)
    if(1==1):
        if(ser.in_waiting >0):
            line = ser.readline()
            print("reading from serial")
            print(line)
            decode = line.decode('unicode_escape') # decoding into string
            with conn.cursor() as cursor:
                x = decode.split() #splitting using space as delimitor
#                 print(x)
                if x[0]=="B": # battery reading
                    print("battery log")
                    level = int(float(x[2]))
                    id = [x[1]]
                    cursor.execute('SELECT * FROM staff WHERE staff_no = %s',id) # checking if record exists
                    y = cursor.fetchall()
                    print(y)
                    y = cursor.rowcount
#                     print(y)
                    if(y!=0):
                        print("updating batt")
                        data = [level, id[0]]
                        cursor.execute('UPDATE staff SET staff_tag_level = %s WHERE staff_no = %s', data)
                        conn.commit()
                    else:
                        print("creating batt")
                        data = [id[0], level]
                        cursor.execute('INSERT INTO staff VALUES (%s,%s)',data)
                        conn.commit()
                else:
#                     print(x)
#                     print(type(x))
                    print("interaction")
                    for_upload = [x[0], x[1], x[2], x[3], today_date]
                    print(for_upload)
                    cursor.execute('INSERT INTO tag_interact (staff_no_1, staff_no_2, interact_duration, interact_time, interact_date) VALUES (%s,%s,%s, %s, %s)',for_upload)
                    conn.commit()
