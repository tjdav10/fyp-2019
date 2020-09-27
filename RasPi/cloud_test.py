import serial
import pymysql.cursors
import string

# ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = pymysql.connect(host='128.199.68.151',
                       user='tim',
                       password='monashtim123',
                       db='hand_hygiene',
                       port=3306,
                       cursorclass=pymysql.cursors.DictCursor)
print("connected")
while 1:
    print('enter battery log')
    x = input()
    x = x.split()
#     print(x)
    with conn.cursor() as cursor:
        id = [x[0]]
        level = x[1]
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
            cursor.execute('INSERT INTO battery VALUES (%s,%s)',x)
            conn.commit()