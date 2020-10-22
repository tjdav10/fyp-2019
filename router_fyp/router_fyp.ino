/*
 * Router BLE program for FYP
 * 
 * Connects to wearables to forward relevant data to Gateway
 * 
 * 
 * TODO:
 *    Write some code to prevent connections spamming
 *    For some reason it stops working after a period of time
 *    It won't connect to peripheral
 */

#include <bluefruit.h>
#include "RTClib.h"

#define MAX_CONN (5)
#define ARRAY_SIZE (10)

char ID[4+1] = "R001";

char last_conn[5];

BLEClientUart clientUart; // bleuart client

RTC_PCF8523 rtc;

// Each peripheral needs its own bleuart client service
typedef struct
{
  char name[16+1];

  uint16_t conn_handle;

  // Each prph need its own bleuart client service
  BLEClientUart bleuart;
} prph_info_t;

typedef struct
{
  char name[4+1];
  uint32_t last_time;
} sent_timer;

const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};

BLEUuid uuid = BLEUuid(CUSTOM_UUID);

char MANU_ID[4];

uint16_t connection;
char peer_name[32] = { 0 };

prph_info_t prphs[MAX_CONN];
sent_timer timer_tracking[ARRAY_SIZE];

// Software Timer for blinking the RED LED
SoftwareTimer blinkTimer;
uint8_t connection_num = 0; // for blink pattern

/*
// Battery level variables
int adcin    = A6;  // A6 == VDIV is for battery voltage monitoring 
int adcvalue = 0;
float mv_per_lsb = 3600.0F/4096.0F; // 12-bit ADC with 3.6V input range
                                    //the default analog reference voltage is 3.6 V
float v_max = 4.06; // the maximum battery voltage
float v_min = 3.00; // min battery voltage
*/
 
void setup()
{
  if (! rtc.begin()) {
//  Serial.println("Couldn't find RTC");
  Serial.flush();
  abort();
  }
  uint16_t MANU_ID = 65535;
  Serial.begin(9600);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
//  Serial.println("starting...");
  rtc.start();
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
  
  Bluefruit.setName(ID);

//  analogReadResolution(12); // Can be 8, 10, 12 or 14


 for (uint8_t m=0; m<ARRAY_SIZE; m++)
 {
  timer_tracking[m].last_time = 0;
  memset(timer_tracking[m].name, 0, sizeof(timer_tracking[m].name));
 }
// Serial.printf("done");
 
  // Init BLE Central Uart Serivce
  for (uint8_t idx=0; idx<MAX_CONN; idx++)
  {
    prphs[idx].conn_handle = BLE_CONN_HANDLE_INVALID;

    // All of BLE Central Uart Serivce
    prphs[idx].bleuart.begin();
    prphs[idx].bleuart.setRxCallback(bleuart_rx_callback);
  }
 
  // Increase Blink rate to different from PrPh advertising mode
  Bluefruit.setConnLedInterval(250);
 
  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);
 
  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.filterUuid(uuid);
//  Bluefruit.Scanner.filterMSD(MANU_ID);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // // 0 = Don't stop scanning after n seconds
}
 
/**
 * Callback invoked when scanner pick up an advertising data
 * @param report Structural advertising data
 */
void scan_callback(ble_gap_evt_adv_report_t* report)
{
//  Serial.println("detected adv packet");
  DateTime now = rtc.now();
  // Need to have some condition here to see if recently connected to this device (maybe a list of recent names)
  uint8_t buffer[4]; // used for names of 4 chars long
  Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)); // puts name of device in buffer
//  Serial.printf("last_conn = %c\n",last_conn);
//  Serial.printf("buffer = %c\n",buffer);
  char strbuff[4+1];
  memcpy(strbuff, buffer, 4);
  strbuff[4] = '\0';
  
  for (uint8_t j; j<ARRAY_SIZE; j++)
  {
    if((strcmp(strbuff, timer_tracking[j].name)==0))
    {
//      Serial.println(timer_tracking[j].name);
      if(now.unixtime() - timer_tracking[j].last_time >=300)
      {
        timer_tracking[j].last_time = now.unixtime();
        Bluefruit.Central.connect(report);
      }
      else
      {
//        Serial.println("recently connected to device");
        goto done;
      }
    }
    else if(timer_tracking[j].name[0] == 0)
    {
      timer_tracking[j].last_time = now.unixtime();
      strcpy(timer_tracking[j].name, strbuff);
      goto connect_to_device;
    }
  }
  connect_to_device:
    Bluefruit.Central.connect(report);
    return;
  done:
    Bluefruit.Scanner.resume();
    return;
}
 
/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  // Find an available ID to use
  int id  = findConnHandle(BLE_CONN_HANDLE_INVALID);

  // Eeek: Exceeded the number of connections !!!
  if ( id < 0 ) return;
  
  prph_info_t* peer = &prphs[id];
  peer->conn_handle = conn_handle;

  Bluefruit.Connection(conn_handle)->getPeerName(peer->name, sizeof(peer->name)-1);

//  Serial.print("Connected to ");
//  Serial.println(peer->name);
  
  if ( peer->bleuart.discover(conn_handle) )
  {
//    Serial.println("uart discovered");
    peer->bleuart.enableTXD();
    Bluefruit.Scanner.start(0);

    /*
    adcvalue = analogRead(adcin);
    sprintf(str, "B %.7s %.2f %.2f %.2f", ID, (adcvalue * mv_per_lsb/1000*2), v_max, v_min); // B at start to indicate battery level, limit id to 4 chars, and voltages to 2 decimal places
    Serial.printf(str);
    */
 
  }else
  {
    
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }  
  connection_num++;
}
 
/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  connection_num--;

  // Mark the ID as invalid
  int id  = findConnHandle(conn_handle);

  // Non-existant connection, something went wrong, DBG !!!
  if ( id < 0 ) return;

  // Mark conn handle as invalid
  prphs[id].conn_handle = BLE_CONN_HANDLE_INVALID;

//  Serial.print(prphs[id].name);
//  Serial.println(" disconnected!");
  Bluefruit.Scanner.start(0);
}
 
/**
 * Callback invoked when uart received data
 * @param uart_svc Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void bleuart_rx_callback(BLEClientUart& uart_svc)
{
  // uart_svc is prphs[conn_handle].bleuart
  uint16_t conn_handle = uart_svc.connHandle();

  int id = findConnHandle(conn_handle);
  prph_info_t* peer = &prphs[id];


  
  char received_string[20+1] = {0};
  while ( uart_svc.available() )
  {
    uart_svc.read(received_string, sizeof(received_string)-1);
    //Serial.print( (char) uart_svc.read() );
//    if(received_string[0] == 'B' || received_string[0] == 'N' || received_string[0] == 'D' || received_string[0] == 'V')
    {
      Serial.print(received_string);
      if(received_string[0] =='B')
      {
//        Serial.println("\nbattery received");
        
//        Bluefruit.disconnect(connection);
      }
      
//      This would be if I had Lora
//      Wire.beginTransmission(4); // transmit to device #8           // LoRa send info to gateway
//      Wire.write(info_from_dispenser);        // sends five bytes
//      Wire.write(";");              // sends deliminator
//      Wire.endTransmission();    // stop transmitting
    }
//    else
    {
      // hand hygiene stuff here
    }
  }
 
  Serial.println();
}
 
void loop()
{
  delay(2);
//  Below is not needed - just for debugging
//  if ( Bluefruit.Central.connected() )
//  {
//    // Not discovered yet
//    if ( clientUart.discovered() )
//    {
//      // Discovered means in working state
//      // Get Serial input and send to Peripheral
//      if ( Serial.available() )
//      {
//        delay(2); // delay a bit for all characters to arrive
//        
//        char str[20+1] = { 0 };
//        Serial.readBytes(str, 20);
//        
//        clientUart.print( str );
//      }
//    }
//  }
}

int findConnHandle(uint16_t conn_handle)
{
  for(int id=0; id<BLE_MAX_CONNECTION; id++)
  {
    if (conn_handle == prphs[id].conn_handle)
    {
      return id;
    }
  }

  return -1;  
}

void blink_timer_callback(TimerHandle_t xTimerID)
{
  (void) xTimerID;

  // Period of sequence is 10 times (1 second). 
  // RED LED will toggle first 2*n times (on/off) and remain off for the rest of period
  // Where n = number of connection
  static uint8_t count = 0;

  if ( count < 2*connection_num ) digitalToggle(LED_RED);
  if ( count % 2 && digitalRead(LED_RED)) digitalWrite(LED_RED, LOW); // issue #98

  count++;
  if (count >= 10) count = 0;
}
