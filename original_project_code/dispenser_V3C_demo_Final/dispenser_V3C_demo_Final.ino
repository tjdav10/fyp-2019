/* Version V1D: use 3 VL53L0X sensors for body detection and 1 VL53L0X sensor for hand detection */
/* V2: using node_record_s structure (record_nodes[4]) to sotre scanned wearable_ID information, and record the one with the highest RSSI value */
/* V3: change sending data structure to arrays of char array instead of a single char array; also replace Arduino String object with char array type */
/* V3B: change the sending data structure back to a single char array; learned more about C string (char array) type */ 
/* V3C: add more timestamps to determine when to connect and disconnect Wearable_ID and send reminder notification to Wearable_ID */
/* V3C_demo: comment uncessary serial.print() functions; aim for field demo */
/* V3C_demo2: modifications based on demo experience on 3rd July in the mock ward; 
              also add two leds: yellow/red for when subject connected but hand not washed, green for hand washed */
/* V3C_demo_Final: The final version for trial test based on my work. */

/* This and previous versions are based on adafruit library 0.9.3 */

#include <bluefruit.h>        // the BLE module: nrf52840 and nrf52832
#include <Wire.h>

#include <VL53L0X.h>              // the VL53L0X sensor lib, from: https://github.com/pololu/vl53l0x-arduino

#define RSSI_NEARBY_VALUE     -70       // the rssi within range limit
#define RSSI_CLOSE_VALUE      -60       // the rssi close limit

#define MAXIMUM_SCANNED_NODES 4         // the maximum num  of nodes scanned for comapring RSSI values
#define RSSI_WIN_SIZE     5             // the window size to average scanned rssi

#define RECORD_NODE_RESET_TIME  5000    // reset the reord node's RSSI values after 5 seconds.

#define LED_G 5
#define LED_R 6


// Custom UUID used to differentiate this device.
// Use any online UUID generator to generate a valid UUID.
// Note that the byte order is reversed ... CUSTOM_UUID
// below corresponds to the follow value:
// e4a6f2a1-2967-4479-be73-f6f4b5cd07f8                this uuid is for the dispensers 
const uint8_t CUSTOM_UUID[] =
{
    0xF8, 0x07, 0xCD, 0xB5, 0xF4, 0xF6, 0x73, 0xBE,
    0x79, 0x44, 0x67, 0x29, 0xA1, 0xF2, 0xA6, 0xE4
};
BLEUuid Dispenser_UUID = BLEUuid(CUSTOM_UUID);

// e75cc8dc-7dfa-49d5-aa50-007d36c08b9a                 this uuid is for the wearables 
const uint8_t CUSTOM_UUID2[] =
{
    0x9A, 0x8B, 0x6C, 0x36, 0x7D, 0x00, 0x50, 0xAA,
    0xD5, 0x49, 0xFA, 0x7D, 0xDC, 0xC8, 0x5C, 0xE7
};
BLEUuid Wearable_UUID = BLEUuid(CUSTOM_UUID2);

/* Add BLE services */
BLEClientUart client_dispenser;             // uart over ble, will connect to wearable_ID as the central
BLEUart dispenser;                          // uart over ble, connected to the gateway as the peripheral

/* distance sensor address */
#define sensor_Hand_ADDRESS 0x30
#define sensor_M_ADDRESS 0x31
#define sensor_R_ADDRESS 0x32
#define sensor_L_ADDRESS 0x33

// set the pins to shutdown
#define SHT_sensor_Hand 9                   // distance sensor for hand detection
#define SHT_sensor_M 10                     // distance sensor in the middle
#define SHT_sensor_R 11                     // distance sensor on the right
#define SHT_sensor_L 12                     // distance sensor on the left

/* the VL53L0X sensor */
VL53L0X dis_sensor_Hand;
VL53L0X dis_sensor_M;
VL53L0X dis_sensor_R;
VL53L0X dis_sensor_L;

/* device information (ID) */
char last_washed_ID[4+1];                                 // the last ID that is connected and detected as hand-washed
char Scanned_Wearable_ID[4+1];                            // used for scanning wearable_ID 
char Connected_Wearable_ID[4+1];                          // the wearable_ID that is connected to the dispenser
char node_washed_from_gateway[19+1] = "BX000";                      // the info about node_washed from gateway; use strstr() to compare Scanned_Wearable_ID with this char array
  
char Dispenser_ID[7+1] = "R001D04";                   // Dispenser_ID as the char array

/* the data structure for storing scanned wearable_ID information */
typedef struct node_record_s                // scanned wearable_ID information
{
  char node_ID[4+1];            // the wearable_ID      e.g. "D001"
  int8_t  node_rssi;            // current RSSI value
  int8_t  previous_node_rssi[RSSI_WIN_SIZE];       // previous RSSI values for average, the windows size is 5
  int8_t  average_node_rssi;    // average RSSI value
  int32_t timestamp;            // the current updated Timestamp 
  bool reset_flag;
} node_record_t;

node_record_t record_nodes[MAXIMUM_SCANNED_NODES];                  // maximum scanned nodes are 4 (default)

/* distance sensor variables */
volatile int distance_M;
volatile int distance_R;
volatile int distance_L;

/* current BLE connection handle */
uint16_t current_conn_handle;

/* The time variables */
volatile unsigned long scan_received_time = 0;          // the scanning received package time
volatile unsigned long conn_time = 0;                   // the BLE connection time
volatile unsigned long disconn_time = 0;                // the BLE disconnection time
volatile unsigned long last_ID_washing_time = 0;        // the last ID hand washing time
volatile unsigned long hand_washing_time_before = 0;    // the time before hand washing
volatile unsigned long hand_washing_time_after = 0;     // the time after hand washing
volatile unsigned long ID_connection_time = 0;          // the time when the ID is connected
volatile unsigned long subject_found_time_before = 0;   // the time in loop() when subject is found
volatile unsigned long subject_found_time_after = 0;    // the time to confirm subject is found; used to break while()

/* the time variables for debug printing time */ 
volatile unsigned long rssi_withinRange_time = 0;       // when found subject within range, print every 1s
volatile unsigned long rssi_close_time = 0;             // when found subject very close, print every 1s
volatile unsigned long subject_found_time = 0;          // when found subject within 0.5 m, print every 1s
volatile unsigned long ignore_ID_time = 0;              // ignore the last hand-washed ID 
volatile unsigned long current_debug_time = 0;          // a general debug flag for current time

/* The state bool flags */
bool RSSI_nearby = false;                           // rssi > -70 dBm, the subject is within range         
bool RSSI_close = false;                            // rssi > -60 dBm, the subject is close, detected by rssi
bool subject_found = false;                         // distance sensor < 1000 mm
bool ID_connected = false;                          // rssi > -60 dBm & distance sensor < 500 mm, true to connect BLE
bool hands_washed = false;                          // hands washed flag     
bool ID_received = false;                           // receive acknowledgement from ID   
bool gateway_received = false;                      // receive acknowledgement from gateway

void setup() 
{
//  Serial.begin(115200);
  // while ( !Serial ) delay(10);   // for nrf52840 with native usb    open serial port to hardware

  Wire.begin();                     // I2C wire

  delay(10);   
  Serial.println("Dispenser as dual role test");
  Serial.println("------------------------------------\n");

  // led indicators
  pinMode(LED_G, OUTPUT);  
  pinMode(LED_R, OUTPUT);

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);


  // the VL53L0X sensor setup                                           // the 4 distance sensors setup
  pinMode(SHT_sensor_Hand, OUTPUT);
  pinMode(SHT_sensor_M, OUTPUT);
  pinMode(SHT_sensor_R, OUTPUT);
  pinMode(SHT_sensor_L, OUTPUT);
  
  // all reset
  digitalWrite(SHT_sensor_Hand, LOW);
  digitalWrite(SHT_sensor_M, LOW);    
  digitalWrite(SHT_sensor_R, LOW);
  digitalWrite(SHT_sensor_L, LOW);    
  delay(10);
  
  // all unreset
  digitalWrite(SHT_sensor_Hand, HIGH);
  digitalWrite(SHT_sensor_M, HIGH);
  digitalWrite(SHT_sensor_R, HIGH);
  digitalWrite(SHT_sensor_L, HIGH);
  delay(10);
  
  // activating sensor_Hand and reseting sensor middle, left, right
  digitalWrite(SHT_sensor_M, LOW);
  digitalWrite(SHT_sensor_R, LOW);
  digitalWrite(SHT_sensor_L, LOW);
  dis_sensor_Hand.setAddress(sensor_Hand_ADDRESS);
  delay(10);
  
  // activating sensor_M
  digitalWrite(SHT_sensor_M, HIGH);
  delay(10);
  dis_sensor_M.setAddress(sensor_M_ADDRESS);
  
  // activating sensor_R
  digitalWrite(SHT_sensor_R, HIGH);
  delay(10);
  dis_sensor_R.setAddress(sensor_R_ADDRESS);
  
  // activating sensor_L
  digitalWrite(SHT_sensor_L, HIGH);
  delay(10);
  dis_sensor_L.setAddress(sensor_L_ADDRESS);

  dis_sensor_Hand.init();
  dis_sensor_M.init();
  dis_sensor_R.init();
  dis_sensor_L.init();
  dis_sensor_Hand.setTimeout(100);
  dis_sensor_M.setTimeout(100);
  dis_sensor_R.setTimeout(100);
  dis_sensor_L.setTimeout(100);

  // lower the return signal rate limit (default is 0.25 MCPS)        // for lang range measurements (up to 2m)
  dis_sensor_Hand.setSignalRateLimit(0.1);
  // increase laser pulse periods (defaults are 14 and 10 PCLKs)
  dis_sensor_Hand.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  dis_sensor_Hand.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);

  dis_sensor_M.setSignalRateLimit(0.1);
  dis_sensor_M.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  dis_sensor_M.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);

  dis_sensor_R.setSignalRateLimit(0.1);
  dis_sensor_R.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  dis_sensor_R.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);

  dis_sensor_L.setSignalRateLimit(0.1);
  dis_sensor_L.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  dis_sensor_L.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);


  // Initialize Bluefruit with maximum connections as Peripheral = 1, Central = 1
  // The Peripheral and Central are to other BLEs, e.g. this dispenser is Peripheral to the gateway         
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(1, 1);
  
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);

  /* Set the device name */
  Bluefruit.setName(Dispenser_ID);                                 // may change the name for our own project

  /* Set the LED interval for blinky pattern on BLUE LED */         // later may turn off this blink to save power
  Bluefruit.setConnLedInterval(1000);

  // Callbacks for Peripheral
  Bluefruit.setConnectCallback(prph_connect_callback);
  Bluefruit.setDisconnectCallback(prph_disconnect_callback);

  // Callbacks for Central (BLE UART service)
  Bluefruit.Central.setConnectCallback(cent_connect_callback);
  Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

  // Configure and Start peripheral BLE Uart Service
  dispenser.begin();
  dispenser.setRxCallback(prph_bleuart_rx_callback);

  // Init BLE Central Uart Serivce
  client_dispenser.begin();
  client_dispenser.setRxCallback(cent_bleuart_rx_callback);

  // reset sacnned_node array
  for (int i=0; i<MAXIMUM_SCANNED_NODES; i++) 
    record_nodes[i] = Reset_Scanned_Node();                   // reset record_nodes

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
  Bluefruit.Scanner.filterUuid(Wearable_UUID);          // only invoke callback if detect Wearable_UUID 
  Bluefruit.Scanner.setInterval(160, 80);                // in units of 0.625 ms   the default is 160, 80
  Bluefruit.Scanner.useActiveScan(true);                // Request scan response data
  Bluefruit.Scanner.start(0);                           // 0 = Don't stop scanning after n seconds

  // Set up and start advertising
  startAdv();                                           // start advertising as the peripheral
}

/* the peripheral advertising setup */
void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  Bluefruit.ScanResponse.addUuid(Dispenser_UUID);               // the scanresponse can be advertised when requested
  Bluefruit.ScanResponse.addName();                             // scanresponse can double the advertisiing package (62 bytes)

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(40, 80);                    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(10);                     // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                               // 0 = Don't stop advertising after n seconds
}

/*------------------------------------------------------------------*/
/* Central
 *------------------------------------------------------------------*/
void scan_callback(ble_gap_evt_adv_report_t* report)
{
  uint8_t len = 0;
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));

  int current_node_id = -1;                                   // the current scanned id, these variables are for node_record_t struct
  bool node_id_assigned = false;

  /* Display the timestamp and device address */
  if (report->type.scan_response)                             // scan_response type information
  {
    scan_received_time = millis();
  }
  else
  {
    scan_received_time = millis();
  }
  
  /* Shortened Local Name */                // the device name may be shortened due to the advertising package size limitation
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    strcpy(Scanned_Wearable_ID, (char*)(buffer));
    memset(buffer, 0, sizeof(buffer));
  }

  /* Complete Local Name */       
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)))      // to find the device name
  {
    strcpy(Scanned_Wearable_ID, (char*)(buffer));
    memset(buffer, 0, sizeof(buffer));
  }

  /* to store scanned wearable_ID information */

  /* Firstly, check whether there is any out-of-time recorded node. If so, reset it. */
  for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
  {
    if ((record_nodes[i].reset_flag == false) && (scan_received_time - record_nodes[i].timestamp > RECORD_NODE_RESET_TIME))       // the record_node has not been updated for RECORD_NODE_RESET_TIME s
    {
      // Serial.print("found out-of-time (>10 s) ID");
      // Serial.print(" ");
      // Serial.print(i);
      // Serial.println("********************************************* Reset it");
      record_nodes[i] = Reset_Scanned_Node();
    }
  }

  /* Secondly, compare with existing recorded Scanned_Wearable_ID, to chech whether this node id is already recorded */
  for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
  {
  //  if (record_nodes[i].node_ID == Scanned_Wearable_ID)
    if (strcmp(record_nodes[i].node_ID, Scanned_Wearable_ID) == 0)          // strcmp = 0 when the two char arrays equal
      {
        // Serial.println("found existing ID");
        current_node_id = i;
        node_id_assigned = true;
        break;
      }
  }

  /* If there is no existing node_ID for this scanned node, check whether there is any record_nodes[] is reset */
  if (node_id_assigned == false)
  {
    for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
    {
      if (record_nodes[i].reset_flag == true)
      {
        // Serial.println("assigned new id");
        current_node_id = i;
        node_id_assigned = true;
        break;
      }
    }
  }

//   /* if there is no existing node_ID and there is no reset ones, we need to remove the lowest rssi node.*/
  
//   // this will be done later. Or just increase MAXIMUM_SCANNED_NODES to avoid this issue :(

  /* Now the scanned node is assigned with an id, update information */
  strcpy(record_nodes[current_node_id].node_ID, Scanned_Wearable_ID);
  record_nodes[current_node_id].node_rssi = report->rssi;
  record_nodes[current_node_id].timestamp = scan_received_time;
  record_nodes[current_node_id].reset_flag = false;
  
  /* To calculate average rssi */
  int temp_rssi = 0;
  for (int i=0; i<RSSI_WIN_SIZE-1; i++)
  {
    record_nodes[current_node_id].previous_node_rssi[i] = record_nodes[current_node_id].previous_node_rssi[i+1];
    temp_rssi += record_nodes[current_node_id].previous_node_rssi[i];
  }
  record_nodes[current_node_id].previous_node_rssi[RSSI_WIN_SIZE-1] = record_nodes[current_node_id].node_rssi;
  temp_rssi += record_nodes[current_node_id].previous_node_rssi[RSSI_WIN_SIZE-1];
  record_nodes[current_node_id].average_node_rssi = temp_rssi / RSSI_WIN_SIZE;

  /* print the current scanned wearable ID for debug */
  // Serial.print("Current scanned Wearable ID: ");
  // Serial.println(record_nodes[current_node_id].node_ID);
  // // Serial.print("RSSI: ");
  // // Serial.println(record_nodes[current_node_id].node_rssi);
  // Serial.print("Average RSSI: ");
  // Serial.println(record_nodes[current_node_id].average_node_rssi);
  // // Serial.print("Scanned time: ");
  // // Serial.println(record_nodes[current_node_id].timestamp);
  // Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");

  /* print all the scanned wearable ID for debug */
  // for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
  // {
  //   if (record_nodes[i].reset_flag == false)
  //   {
  //     Serial.print("All the scanned Wearable ID: ");
  //     Serial.print(i);
  //     Serial.print(" ");
  //     Serial.println(record_nodes[i].node_ID);
  //     // Serial.print("RSSI: ");
  //     // Serial.println(record_nodes[i].node_rssi);
  //     Serial.print("Average RSSI: ");
  //     Serial.println(record_nodes[i].average_node_rssi);
  //     // Serial.print("Scanned time: ");
  //     // Serial.println(record_nodes[i].timestamp);
  //     Serial.println();
  //   }
  // }
  // Serial.println("---------------------------------");

  /* now we need to find the closest node in the record_nodes[] */        // a more advanced way is to sort the record_nodes[]
  int maximum_RSSI_Node_ID = -1;
  /* Firstly, find one of the record_nodes[] that is valid*/
  for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
  {
    if (record_nodes[i].reset_flag == false)
    {
      maximum_RSSI_Node_ID = i;
    }
  }

  /*Then, compare the record_nodes[maximum_RSSI_Node_ID] with other record_nodes[] to find the true maximum_RSSI_Node_ID */
  for (int i=0; i<MAXIMUM_SCANNED_NODES; i++)
  {
    if((record_nodes[i].reset_flag == false) && (record_nodes[i].average_node_rssi > record_nodes[maximum_RSSI_Node_ID].average_node_rssi))
    {
      maximum_RSSI_Node_ID = i;
    }
  }

  /*Now we know the cloeset node ID */
  char closest_node_ID[4+1] = "X000";
  if (maximum_RSSI_Node_ID >= 0)
  {
    strcpy(closest_node_ID, record_nodes[maximum_RSSI_Node_ID].node_ID);
    // Serial.print("The closest Node ID (according to RSSI): ");
    // Serial.println(closest_node_ID);
  }

  /* to connect with the closest wearable_ID */
  if (strcmp(Scanned_Wearable_ID, last_washed_ID) != 0 || scan_received_time - last_ID_washing_time > 3000)       // ignore the last washed ID for 3 s; otherwise it will be connected  
  {                                                                                                               // because node_washed_from_gateway updating needs time       
    if (record_nodes[current_node_id].average_node_rssi > RSSI_NEARBY_VALUE)      
    {
      RSSI_nearby = true;
      // if ((millis() - rssi_withinRange_time > 2000) && record_nodes[current_node_id].average_node_rssi < RSSI_CLOSE_VALUE)           // do not print "within range" if it is "close" 
      // {
      //   Serial.print(Scanned_Wearable_ID);
      //   Serial.println (" is within range (RSSI > -70 dBm). ");
      //   rssi_withinRange_time = millis();
      // }
      // do something to enable the distance sensor and run it in the loop
      // add more functions
    }
    else 
    {
      RSSI_nearby = false;
    }

    if (record_nodes[current_node_id].average_node_rssi > RSSI_CLOSE_VALUE)
    {
      RSSI_close = true;
      // if (millis() - rssi_close_time > 2000)                                 // serial.print() for debugging
      // {
      //   Serial.print(Scanned_Wearable_ID);
      //   Serial.println(" is close (RSSI > -60 dBm). Once the distance sensors detect the subject, it is ready to connect");
      //   rssi_close_time = millis();
      // }
      // add more functions
    }
    else
    {
      RSSI_close = false;
    }

    char *found;
    found = strstr(node_washed_from_gateway, Scanned_Wearable_ID);    // check whether Scanned_Wearable_ID is in the node_washed_from_gateway; if not, it can be connected
    // RSSI_close; subject_found; it is the cloeset node; it is not in the node_washed_from_gateway
    // then connect the wearable_ID to the dispenser
    if (RSSI_close && subject_found && strcmp(record_nodes[current_node_id].node_ID, closest_node_ID) == 0 && !found)
    {
      // Connect to device
      Bluefruit.Central.connect(report);
      digitalWrite(LED_R, HIGH);            // when connected, turn on red led
    }
  }

  // else                                                          // if the ID just washed hands, ignore the last ID for 20 s. Print some debug information
  // {
  //   if (millis() - ignore_ID_time > 2000)
  //   { 
  //     Serial.print(last_washed_ID);
  //     Serial.println(" has washed his/her hands, ignore the ID for 20 seconds.");
  //     ignore_ID_time = millis();
  //   }
  // }

  // For Softdevice v6: after received a report, scanner will be paused
  // We need to call Scanner resume() to continue scanning
  Bluefruit.Scanner.resume();
//  Bluefruit.Scanner.start(0);                           // 0 = Don't stop scanning after n seconds
}

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void cent_connect_callback(uint16_t conn_handle)
{
  ID_connection_time = millis();                        // record the time when the wearable ID is connected

  Bluefruit.Gap.getPeerName(conn_handle, Connected_Wearable_ID, sizeof(Connected_Wearable_ID));     // the connected wearable_ID
  Serial.print(Connected_Wearable_ID);
  Serial.println(" is connected");
  
  current_conn_handle = conn_handle;                    // current BLE connection handle

  ID_connected = true;

  // Serial.print("Discovering BLE Uart Service ... ");
  if ( client_dispenser.discover(conn_handle) )
  {
    // Serial.println("Found it");
    // Serial.println("Enable TXD's notify");
    client_dispenser.enableTXD();
    // Serial.println("Ready to receive from peripheral");
  }
  // else
  // {
  //   Serial.println("Found NONE");
  //   // disconect since we couldn't find bleuart service
  //   Bluefruit.Central.disconnect(conn_handle);
  // }  

}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  
  ID_connected = false;
  Serial.print(last_washed_ID);
  Serial.print(" is now ");
  Serial.println("Disconnected");

  digitalWrite(LED_G, LOW);
  digitalWrite(LED_R, LOW);
  
}

void cent_bleuart_rx_callback(BLEClientUart& uart_svc)
{
  String Waiting_BLEuart;                             // a good way to read string without knowing its size;
  while (uart_svc.available()) 
  {                      
    uint8_t inChar = (uint8_t)uart_svc.read();        // ble uart send uint8_t
    Waiting_BLEuart += (char)inChar;                  // uint8_t -> char -> string
  }

  Serial.print("Acknowledgement from ID: ");
  Serial.println(Waiting_BLEuart); 

  if (Waiting_BLEuart == "rec")                       // rec: received acknowledgement from wearable ID
  {
    ID_received = true;
  }

}


/*------------------------------------------------------------------*/
/* Peripheral
 *------------------------------------------------------------------*/
void prph_connect_callback(uint16_t conn_handle)
{
  char peer_name[32] = { 0 };
  Bluefruit.Gap.getPeerName(conn_handle, peer_name, sizeof(peer_name));

  Serial.print("[Prph] Connected to ");
  Serial.println(peer_name);
}

void prph_disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("[Prph] Disconnected");
 
}

void prph_bleuart_rx_callback(void)
{
  char info_from_gateway[19+1];
  while (dispenser.available()) 
  {                      
    dispenser.read(info_from_gateway, sizeof(info_from_gateway)-1);
  } 
  Serial.print("Info from gateway: ");
  Serial.println(info_from_gateway);

  /* later for different info types, we can use case function */
  if (info_from_gateway[0] == 'X')          // if info_from_gateway starts with 'X', then this info is received acknowldegement 
  {
    gateway_received = true;
  }

  if (info_from_gateway[0] == 'B')          // if info_from_gateway starts with 'B', then this info is info_node_washed
  {
    strcpy(node_washed_from_gateway, info_from_gateway);
    Serial.print("node_washed_from_gateway: ");
    Serial.println(node_washed_from_gateway);
  }

  memset(info_from_gateway, 0, sizeof(info_from_gateway));
}

/*------------------------------------------------------------------*/
/* Scanned wearable_ID information 
 *------------------------------------------------------------------*/

node_record_t Reset_Scanned_Node()
{
  node_record_t node;
  // char default_ID[4+1] = {'X', '0', '0', '0'};
  // node.node_ID = "X000";
  strcpy(node.node_ID, "X000");                             // be extremely careful when dealing with char arrays or strings
  node.node_rssi = -70;
  // node.node_avgRSSI.reset(-70);
  for (int i =0; i<5; i++) node.previous_node_rssi[i] = -70;
  node.average_node_rssi = -70;
  node.reset_flag = true;
  node.timestamp = 0;

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

/*------------------------------------------------------------------*/
/* loop
 *------------------------------------------------------------------*/
void loop() 
{
  /* the first method for body detection: detect once */

  // if (RSSI_nearby)                                                  // enable and read distance sensor data
  // {
  //   distance_M = dis_sensor_M.readRangeSingleMillimeters();
  //   distance_R = dis_sensor_R.readRangeSingleMillimeters();
  //   distance_L = dis_sensor_L.readRangeSingleMillimeters();
  //   if (distance_M < 500 || distance_R < 500 || distance_L < 500)                                             // subject is within 500 mm, as a reference
  //   {
  //     subject_found = true;
  //     if (millis() - subject_found_time > 2000)
  //     {
  //       // Serial.print(Scanned_Wearable_ID);
  //       // Serial.println("Subject is detected by distance sensor.");
  //       // Serial.print("Distance L: ");
  //       // Serial.print(distance_L);
  //       // Serial.print(" Distance M: ");
  //       // Serial.print(distance_M);
  //       // Serial.print(" Distance R: ");
  //       // Serial.println(distance_R);
  //       subject_found_time = millis();
  //     }
  //   }
  //   else
  //   {
  //     subject_found = false;
  //   }
  // }

  /* the second method for body detection: last for more than xx ms */
  if (RSSI_nearby)                                                  // enable and read distance sensor data
  {
    subject_found_time_before = millis();
    while (dis_sensor_M.readRangeSingleMillimeters() < 500 
            || dis_sensor_R.readRangeSingleMillimeters() < 500 
            || dis_sensor_L.readRangeSingleMillimeters() < 500)     // subject is within 500 mm, as a reference
    {
      if(millis()-subject_found_time_before > 200) break;
      delay(10);
    }
    subject_found_time_after = millis();

    if (subject_found_time_after - subject_found_time_before > 150)        // subject is found for more than 300 mm, as a reference
    {
      subject_found = true;
    }
    else
    {
      subject_found = false;
    }
  }

  current_debug_time = millis();
  if (ID_connected && (current_debug_time - ID_connection_time < 5000))                // when the ID is connected and the connection time is within 5 s
  {
    // enable the hand proximity sensor                                          
    hand_washing_time_before = millis();
    while (dis_sensor_Hand.readRangeSingleMillimeters() < 140)                          // 100 mm is only reference 
    {
    //  Serial.println("Hands are detected.");
    //  delay(100);
      if(millis()-hand_washing_time_before > 1000) break;
      delay(10);
    }
    hand_washing_time_after = millis();

    if (hand_washing_time_after - hand_washing_time_before > 500)          // detect hands for more than 500 ms
    {
      Serial.println("Hands are now washed!");
      hands_washed = true;
      
      // send notification to ID and wait for acknowledgement
      if ( Bluefruit.Central.connected() )
      {
        if ( client_dispenser.discovered() )
        {
          uint8_t write_buf[] = "hands_washed";
          // uint8_t write_buf[] = "1";

          digitalWrite(LED_R, LOW);         // when hand washed, turn off red led; turn on green LED for a short time
          digitalWrite(LED_G, HIGH);

          Serial.println("Sending hand-washed confirmation to Wearable_ID...");
          for (int i = 0; i < 10; i++)
          {
            client_dispenser.write(write_buf, sizeof(write_buf)-1);
            delay(200);                                                     // delay for a while, wait for acknowldegement
            if (ID_received) break;                             // after receiving acknowledgement, break the for loop;
                                                                            // or wait for 10 cycles (100ms)
          }
          ID_received = false;                                  // reset ID_received
        }
      }

      // send hand_washed info to gateway
      char hand_washing_info_to_gateway[19+1];                  // based on experience, this BLE can send up to 20 bits without changing line; need to reset this char array after sending

      strcpy(hand_washing_info_to_gateway, "A");                      // "A" as this info type
      strcat(hand_washing_info_to_gateway, Dispenser_ID);              // "R001D01" for dispenser
      strcat(hand_washing_info_to_gateway, Connected_Wearable_ID);     // "D001" for doctor
      strcat(hand_washing_info_to_gateway, "Y");                       // 'Y' for hand-washed
      
      Serial.println("Sending hand-washed information to gateway...");
      for (int i=0; i<10; i++)
      {
        dispenser.print(hand_washing_info_to_gateway);  
        delay(200);                                                     // delay for a while, wait for acknowldegement
        if (gateway_received) break;                                    // after receiving acknowledgement, break the for loop;                                                                       // or wait for 10 cycles (100ms)
      }
      gateway_received = false;                                         // reset ID_received
      memset(hand_washing_info_to_gateway, 0, sizeof(hand_washing_info_to_gateway));    // reset char array: hand_washing_info_to_gateway 

      // disconnect ID
      Bluefruit.Central.disconnect(current_conn_handle);                    // this should be done after receiving acknowledgement
      strcpy(last_washed_ID, Connected_Wearable_ID);
      last_ID_washing_time = millis();
      delay(200);                                                           // it's important to delay a while to enter the disconnection callback

      digitalWrite(LED_G, LOW);             // turn off green LED;
    }
  }
  else if (ID_connected && (current_debug_time - ID_connection_time >= 5000))                 // when the ID is connected, the ID do not wash hand for 5 seconds
  {
    // send hands_not_washed notification to ID and wait for acknowledgement
    if ( Bluefruit.Central.connected() )
    {
      if ( client_dispenser.discovered() )
      {
        uint8_t write_buf2[] = "not_washed";

        Serial.println("Sending hands-not-washed notification to Wearable_ID...");
        for (int i = 0; i < 10; i++)
        { 
          client_dispenser.write(write_buf2, sizeof(write_buf2)-1);
          delay(100);                                                     // delay for a while, wait for acknowldegement
          if (ID_received) break;                             // after receiving acknowledgement, break the for loop;
                                                                          // or wait for 10 cycles (100ms)
        }
        ID_received = false;                                  // reset ID_received
      }                                                       // it's important to delay a while to enter the disconnection callback
    }

    // send hands_not_washed info to gateway
      char hand_washing_info_to_gateway[19+1];
      
      strcpy(hand_washing_info_to_gateway, "A");                      // "A" as this info type
      strcat(hand_washing_info_to_gateway, Dispenser_ID);              // "R001D01" for dispenser
      strcat(hand_washing_info_to_gateway, Connected_Wearable_ID);     // "D001" for doctor
      strcat(hand_washing_info_to_gateway, "N");                       // 'N' for hand-not-washed

      Serial.println("Sending hands_not_washed information to gateway...");
      for (int i=0; i<10; i++)
      {
        dispenser.print(hand_washing_info_to_gateway);  
        delay(100);                                                     // delay for a while, wait for acknowldegement
        if (gateway_received) break;                                    // after receiving acknowledgement, break the for loop;                                                                       // or wait for 10 cycles (100ms)
      }
      gateway_received = false;                                         // reset ID_received
      memset(hand_washing_info_to_gateway, 0, sizeof(hand_washing_info_to_gateway));    // reset char array: hand_washing_info_to_gateway 

    // disconnect ID
    Bluefruit.Central.disconnect(current_conn_handle); 
    delay(100); 

    digitalWrite(LED_R, LOW);         // turn off red led 
  }

  delay(10);
}
