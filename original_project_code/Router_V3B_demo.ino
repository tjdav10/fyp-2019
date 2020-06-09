 /* This is the gateway application. */
 /* V1B: add peropheral pool to connect 4 dispenser peripheral BLEs */
 /* V2: add node_washed_t struct to store hand washed ID information, and process received info from dispensers */
 /* V2B: add more function to process received info from dispensers */

 /* V3: version 3 is based on Adafruit nRF52 lib Version 0.11.0; V2 and previous versions are based on 0.9.3*/
 /* V3B: connection as contral is limited to 4 due to SRAM size. Try to reduce code sram */
 /* V3B_demo: comment uncessary serial.println(); used for demo purpose */
 /* Router_V3B_demo: add Fan's LoRa transmission; modifications based on field test in mock ward on 3rd July */
 
#include <Wire.h>
#include <bluefruit.h>        // the BLE module: nrf52840 and nrf52832

#define NUM_NODE_WASHED   4

#define BLE_CENTRAL_CONN_Taiyang 5 // previous official version is 4; now official removed this, and this BLE_CENTRAL_MAX_CONN is only for this file 

// Custom UUID used to differentiate this device.
// Use any online UUID generator to generate a valid UUID.
// Note that the byte order is reversed ... CUSTOM_UUID
// below corresponds to the follow value:
// e4a6f2a1-2967-4479-be73-f6f4b5cd07f8                this uuid is for the dispensers 
const uint8_t CUSTOM_UUID[] =
{
    0xF8, 0x07, 0xCD, 0xB5, 0xF4, 0xF6, 0x73, 0xBE,
    0x79, 0x44, 0x67, 0x29, 0xA1, 0xF2, 0xA6, 0xE4            // the last was 0xE4, changed to 0x2B for Fan gateway test
};
BLEUuid Dispenser_UUID = BLEUuid(CUSTOM_UUID);                        // the gateway will connect dispensers with its uuid

// Struct containing peripheral info, for multiple peripheral connection
typedef struct
{
  char name[7+1];                      // the dispenser peripheral name is as: "R001D01"
  uint16_t conn_handle;
  // Each prph need its own bleuart client service
  BLEClientUart bleuart;
} prph_info_t;

/* Peripheral info array (one per peripheral device) */
prph_info_t dispenser_prphs[BLE_CENTRAL_CONN_Taiyang];            // BLE_CENTRAL_CONN_Taiyang=4; the BLEClientUart service to connect dispenser peripherals

typedef struct node_washed_s                          // to record the hand_wahsed node information 
{
  char node_ID[4+1];
  int32_t last_wash_time;
  bool default_flag;
} node_washed_t;

node_washed_t node_washed[NUM_NODE_WASHED];           // node_wahsed num default is 5

char info_node_washed[19+1];                          // hand washed info to send to dispensers
char info_previous[19+1];                             // previous hand washing info for comparison

// Software Timer for blinking the RED LED
//SoftwareTimer blinkTimer;
uint8_t connection_num = 0; // for blink pattern

char gateway_ID[4+1] = "G001";              // the gateway_ID for setname function

/* current BLE connection handle */
uint16_t current_conn_handle;

void setup() 
{
  Serial.begin(115200);
//  while ( !Serial ) delay(10);   // for nrf52840 with native usb    open serial port to hardware

  delay(10);   
  Wire.begin(); // join i2c bus (address optional for master)

//  Serial.println("Gateway test for multiple connection with dispenser peripherals.");
//  delay(10);
//  Serial.println("------------------------------------\n");
//  delay(10);

  Bluefruit.configUuid128Count(5);  // the bluefruit.cpp default is 10; reduce configuration to save sram for 5 connections; can modify bootloader to do this later.
  Bluefruit.configAttrTableSize(512);   //
  Bluefruit.configCentralBandwidth(BANDWIDTH_LOW);
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 5);            // the central can connect up to 4 peripheral dispensers
  
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);

  /* Set the device name */
  Bluefruit.setName(gateway_ID);                                 // may change the name for our own project

  /* Set the LED interval for blinky pattern on BLUE LED */
//  Bluefruit.setConnLedInterval(250);

  // Init peripheral pool                                         // for multiple connections with peripherals
  for (uint8_t idx=0; idx<BLE_CENTRAL_CONN_Taiyang; idx++)
  {
    // Invalid all connection handle
    dispenser_prphs[idx].conn_handle = BLE_CONN_HANDLE_INVALID;
    
    // All of BLE Central Uart Serivce
    dispenser_prphs[idx].bleuart.begin();                       // these are still BLEClientUart service
    dispenser_prphs[idx].bleuart.setRxCallback(bleuart_rx_callback);
  }

  // Callbacks for Central (BLE UART service)
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  // reset node_wahsed information
  for(int i=0; i<NUM_NODE_WASHED; i++)
    node_washed[i] = Reset_Node_washed();

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Filter out packet with a min rssi
   * - Interval = 100 ms, window = 50 ms
   * - Use active scan (used to retrieve the optional scan response adv packet)
   * - Start(0) = will scan forever since no timeout is given
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.filterRssi(-80);                    // the default is -80
  Bluefruit.Scanner.filterUuid(Dispenser_UUID);         // only invoke callback if detect Dispenser_UUID 
  Bluefruit.Scanner.setInterval(80, 40);                // in units of 0.625 ms   the default is 160, 80
  Bluefruit.Scanner.useActiveScan(true);                // Request scan response data
  Bluefruit.Scanner.start(0);                           // 0 = Don't stop scanning after n seconds

//  Serial.println("Scanning ...");
}


void scan_callback(ble_gap_evt_adv_report_t* report)
{
  Bluefruit.Central.connect(report);                               // since the scanned signal has been filtered, just connect to the appropriate device

  // For Softdevice v6: after received a report, scanner will be paused
  // We need to call Scanner resume() to continue scanning
//  Bluefruit.Scanner.resume();
  if (connection_num < 5)
  {
    Bluefruit.Scanner.start(0);     // continue scanning until 5 connections
  }
}


/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  // Find an available ID to use
  int id  = findConnHandle(BLE_CONN_HANDLE_INVALID);                   // BLE_CONN_HANDLE_INVALID: the available connection ID

  // Eeek: Exceeded the number of connections !!!
  if ( id < 0 ) return;
  
  prph_info_t* peer = &dispenser_prphs[id];
  peer->conn_handle = conn_handle;
  
//  Bluefruit.Gap.getPeerName(conn_handle, peer->name, 32);
  Bluefruit.Connection(conn_handle)->getPeerName(peer->name, sizeof(peer->name)-1);

  Serial.print("Gateway is connected to ");
  Serial.println(peer->name);

//  Serial.println(peer_name);

  // Serial.print("Discovering BLE UART service ... ");

  if ( peer->bleuart.discover(conn_handle) )
  {
    // Serial.println("Found it");
    // Serial.println("Enabling TXD characteristic's CCCD notify bit");
    peer->bleuart.enableTXD();

//    Serial.println("Continue scanning for more peripherals");
    Bluefruit.Scanner.start(0);
  }
    
  connection_num++;
  Serial.println(connection_num);

  if (connection_num < 5)
  {
    Bluefruit.Scanner.start(0);     // continue scanning for more peripherals
  }
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
  dispenser_prphs[id].conn_handle = BLE_CONN_HANDLE_INVALID;

  Serial.print(dispenser_prphs[id].name);
  Serial.println(" disconnected!");
}

void bleuart_rx_callback(BLEClientUart& uart_svc)
{
  // uart_svc is dispenser_prphs[conn_handle].bleuart
  uint16_t conn_handle = uart_svc.connHandle();

  int id = findConnHandle(conn_handle);               // to find the right connection handle
  prph_info_t* peer = &dispenser_prphs[id];

  // read the info from dispenser_prphs
  char info_from_dispenser[19+1];
  while (uart_svc.available()) 
  {                      
    uart_svc.read(info_from_dispenser, sizeof(info_from_dispenser)-1);
  }

  // Print sender's name and info             // print this hand washing info for debugging 
//  Serial.printf("[From %s]: ", peer->name);
  delay(200);
  Serial.println(info_from_dispenser);
  Wire.beginTransmission(4); // transmit to device #8           // LoRa send info to gateway
  Wire.write(info_from_dispenser);        // sends five bytes
  Wire.write(";");              // sends deliminator
  Wire.endTransmission();    // stop transmitting
//  Serial.println();
//  Serial.printf("Sending received confirmation to %s \n", peer->name);
//  Serial.println();
  uint8_t write_buf[] = "X";   
  peer->bleuart.write(write_buf, sizeof(write_buf));     // send received acknowledgement 'X' to the dispenser

  if (info_from_dispenser[0] == 'A')                   // if info_from_dispenser starts with 'A', then it is the hand_washing_info: "AR001D01N002Y"
  { 
    if (info_from_dispenser[12] == 'Y')               // this is just checking a char 'Y', whether the subject washes hands
    {
      char temp_id[4+1];
      get_substring(temp_id, info_from_dispenser, 8, 11);          // get substring "N002" to temp_id
      
      bool node_ID_recoreded = false;
      /* first, check whether this wearable ID is recorded */
      for (int i =0; i<NUM_NODE_WASHED; i++)
      {
        if (node_washed[i].default_flag == false)
        {
          if (strcmp(node_washed[i].node_ID, temp_id) == 0)
          {
            node_ID_recoreded = true;
            break;
          }  
        }  
      }

      /*if not recoreded yet, store this subject hand washed information in the node_washed[] */
      if (node_ID_recoreded == false)
      {
        for (int i =0; i<NUM_NODE_WASHED; i++)                   
        {
          if (node_washed[i].default_flag == true)         // this node_wahsed[i] is the default value
          {
            strcpy(node_washed[i].node_ID, temp_id);
            node_washed[i].default_flag = false;
            node_washed[i].last_wash_time = millis() / 1000;    // in seconds
            break;                                        // break this for loop
          }
        }
      }

//      for (int i =0; i<NUM_NODE_WASHED; i++)          // print the hand_washed struct array for debugging
//      {
//        Serial.println(node_washed[i].node_ID);
//      }
    }
  }
}

/**
 * Find the connection handle in the peripheral array
 * @param conn_handle Connection handle
 * @return array index if found, otherwise -1
 */
int findConnHandle(uint16_t conn_handle)
{
  for(int id=0; id<BLE_CENTRAL_CONN_Taiyang; id++)
  {
    if (conn_handle == dispenser_prphs[id].conn_handle)
    {
      return id;
    }
  }

  return -1;            // more than BLE_CENTRAL_CONN_Taiyang(4) connections      
}


/**
 * Software Timer callback is invoked via a built-in FreeRTOS thread with
 * minimal stack size. Therefore it should be as simple as possible. If
 * a periodically heavy task is needed, please use Scheduler.startLoop() to
 * create a dedicated task for it.
 * 
 * More information http://www.freertos.org/RTOS-software-timer.html
 */
//void blink_timer_callback(TimerHandle_t xTimerID)
//{
//  (void) xTimerID;
//
//  // Period of sequence is 10 times (1 second). 
//  // RED LED will toggle first 2*n times (on/off) and remain off for the rest of period
//  // Where n = number of connection
//  static uint8_t count = 0;
//
//  if ( count < 2*connection_num ) digitalToggle(LED_RED);
//  if ( count % 2 && digitalRead(LED_RED)) digitalWrite(LED_RED, LOW); // issue #98
//
//  count++;
//  if (count >= 10) count = 0;
//}


/*------------------------------------------------------------------*/
/* Node_washed information 
 *------------------------------------------------------------------*/
node_washed_t Reset_Node_washed()
{
  node_washed_t node;
  strcpy(node.node_ID, "X000");                             // be extremely careful when dealing with char arrays or strings
  node.last_wash_time = 0;
  node.default_flag = true;
  return node;
}

/*------------------------------------------------------------------*/
/* to get a substring from a C string (char array)
 *------------------------------------------------------------------*/
void get_substring(char *dest, char *src, int start_index, int end_index)
{
  for (int i=0; i<end_index-start_index+1; i++)
  {
    dest[i] = src[start_index+i];
  }
  dest[end_index-start_index+1] = '\0';       // the null terminator
}

/**
 * send info to all connected dispensers
 */
//void sendAll(const char* str)       // example one
void sendAll(const char str[19+1])
{
//  Serial.print("[Send to All]: ");
//  Serial.println(str);

  for(uint8_t id=0; id < BLE_CENTRAL_CONN_Taiyang; id++)
  {
    prph_info_t* peer = &dispenser_prphs[id];
    if ( peer->bleuart.discovered() )
    {
      peer->bleuart.print(str);
//      peer->bleuart.write(str, sizeof(str)-1);
      delay(10);
    }
  }
}

void loop() 
{
  /* to check whether there is expired node_washed info (last_wash_time > 30 s) */
  for (int i =0; i<NUM_NODE_WASHED; i++)          
  {
    if (node_washed[i].default_flag == false)                 // contains hand washed info
    {
      if (millis() / 1000 - node_washed[i].last_wash_time > 15)
      {
        node_washed[i] = Reset_Node_washed();
      }
    }
  }

  /* to send node_washed to the dispensers in the same room */
  strcpy(info_node_washed, "B");                              // info type B: the node_washed info
  for (int i=0; i<NUM_NODE_WASHED; i++)          
  {
    if (node_washed[i].default_flag == false)                 // contains hand washed info
    {
      strcat(info_node_washed, node_washed[i].node_ID);
    }
  }
  strcat(info_node_washed, "b");

//  Serial.print("Before sending: ");
//  Serial.print(info_node_washed);
//  Serial.print(",");
//  Serial.println(info_previous);
  
  if (strcmp(info_node_washed, info_previous) != 0)                           // info_node_washed more than "B", so send this info to dispensers
  {
    sendAll(info_node_washed);
    
//    Serial.print("After sending, sent info: ");
//    Serial.print(info_node_washed);
//    Serial.print(",");
//    Serial.print("previous info: ");
//    Serial.println(info_previous);
    
  }
  strcpy(info_previous, info_node_washed);
  
//  Serial.print("After sending: ");
//  Serial.print(info_node_washed);
//  Serial.print(",");
//  Serial.println(info_previous);
  
  memset(info_node_washed, 0, sizeof(info_node_washed));      // reset info_node_washed after sending

  delay(1000);
//  Serial.println("loop");
}
