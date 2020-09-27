import serial
import pymysql.cursors
import string

ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = pymysql.connect(host='128.199.68.151',
                       user='tim',
                       password='monashtim123',
                       db='hand_hygiene',
                       port=3306,
                       cursorclass=pymysql.cursors.DictCursor)
print("connected")
while 1:
    if(ser.in_waiting >0):
        line = ser.readline()
        decode = line.decode('utf-8') # decoding into string
        with conn.cursor() as cursor:
            x = decode.split() #splitting using space as delimitor
            print(x)
            if x[0]=="B": # battery reading
                v_meas = float(x[2])
                v_max = float(x[3])
                v_min = float(x[4])
                level = int((v_meas-v_min)/(v_max-v_min)*100) # computing battery as %
                id = [x[1]]
                cursor.execute('SELECT * FROM battery WHERE id = %s',id) # checking if record exists
                y = cursor.fetchall()
                print(y)
                y = cursor.rowcount
                print(y)
                if(y!=0):
                    print("updating")
                    data = [level, id[0]]
                    cursor.execute('UPDATE battery SET level = %s WHERE id = %s', data)
                    conn.commit()
                else:
                    print("creating")
                    data = [id[0], level]
                    cursor.execute('INSERT INTO battery VALUES (%s,%s)',data)
                    conn.commit()
            elif (len(decode)>1):# if its interaction log - todo
                
                
                
                
            elif (len(decode)==1):# if its hand hygiene log - do original code stuff