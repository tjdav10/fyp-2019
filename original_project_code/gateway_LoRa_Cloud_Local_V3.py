import serial
import binascii
import time
import requests
import json
import pymysql

def role_identification(x):
    return{'D': "Doctor",
           'N': "Nurse",
           'V': "Visitor",
           }[x]

### SEND DATA TO CLOUD DB BY RESTFUL API
username = r'hygiene_user'
psswrd = 'Hq9oed6Y7'
URL_INS_ROOM = 'https://kpc4ybfy2db.adb.us-ashburn-1.oraclecloudapps.com/ords/hygiene_user/records/messages'

def requestTableData(url):
    resp = requests.get(url,auth=(username, psswrd))
    #if resp.status_code != 200:
     #   raise Exception('GET {} {}'.format(resp.status_code))
    #print(resp.json()['items'])
    return resp.json()['items']
def insertTableData(urltable,data):
    headers = {'Content-Type':'application/json'}
    #print(headers)
    resp = requests.post(url=urltable,json=data,headers=headers,auth=(username, psswrd))
    print('Status code:',resp.status_code)
    #if resp.status_code != 200:
     #  raise Exception('POST /records/ {}'.format(resp.status_code))
def insertData(data):
    insertTableData(URL_INS_ROOM,data)
###

### Store data into local mysql DB
def store_mysql(store_string):
    cursor = connection.cursor()
    #Create a new record
    # b = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    b = time.time()
    sql = "INSERT INTO `hand_tb1` (`rawdata`,`timestamp`) VALUES (%s,NOW())"
    cursor.execute(sql, (store_string))
    # connection is not autocommit by default. So you must commit to save
    # your changes.
    connection.commit()
###

SerialCom = serial.Serial('/dev/ttyUSB0', baudrate=9600)
print("START")

connection = pymysql.connect(
  "localhost",
  "admin",  #username
  "admin",	#password
  "hand_wash_db1"
)
print("sql connected")

data = dict()

#read serial from connected LoRa module and insert to Oracle SQL database
def get_values():
    if SerialCom.inWaiting() > 0:
        my_data = SerialCom.readline()
        data = str(my_data, 'utf-8')
        store_mysql(data)

        line = my_data.decode()
        # sensor_data = line.split(',')
        line = line.strip()
        input_array = list(line)
        Room_ID = ''.join([input_array[2], input_array[3], input_array[4]])
        Dispenser_ID = ''.join([input_array[6], input_array[7]])
        Role_ID = ''.join([input_array[9], input_array[10], input_array[11]])
    
        data = {
                "Time": time.strftime("%H:%M:%S"),
                "Datetime": time.strftime("%Y/%m/%d %H:%M:%S"),
                "Room": Room_ID,
                "Dispenser": Dispenser_ID,
                "Role": role_identification(input_array[8]),
                "Role_ID": input_array[8],
                "ID": Role_ID,
                "Hand_washing": input_array[12]}  # for ble, the last one is null
        # print(data)
		
        data_out = json.dumps(data)
        insertData(data)
        
while True:
    get_values()
    #time.sleep(5)

    
serial_port.close()
