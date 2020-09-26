import serial
import pymysql
import string

# ser = serial.Serial('/dev/ttyACM0', 115200) # opening serial port, ACM0 is top left USB port
conn = pymysql.connect(host='68.183.123.37',
                       user='tim',
                       password='monashtim123',
                       db='db',
                       port=8081,
                       cursorclass=pymysql.cursors.DictCursor)
print("connected")
while 1:
    print('enter battery log')
    x = input()
    x = x.split()
    print(x)
    with conn.cursor() as cursor:
        id = [x[0]]
        level = x[1]
        cursor.execute('SELECT COUNT(1) FROM battery WHERE id = ?',id) # checking if record exists
        y = cursor.fetchone()
        if(y[0]==1):
            data = [level, id[0]]
            print(data)
            cursor.execute('UPDATE battery SET level = ? WHERE id = ?', data)
            conn.commit()
        else:
             cursor.execute('INSERT INTO battery VALUES (?,?)',x)
             conn.commit()