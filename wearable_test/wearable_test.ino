#include <bluefruit.h>

#define feedback_pin 9
#define ARRAY_SIZE 4  //how many records stored
// Custom UUID used to differentiate this device.
// Use any online UUID generator to generate a valid UUID.
// Note that the byte order is reversed ... CUSTOM_UUID
// below corresponds to the follow value:
// e75cc8dc-7dfa-49d5-aa50-007d36c08b9a               this uuid is for the wearable ID
const uint8_t CUSTOM_UUID[] =
{
    0x9A, 0x8B, 0x6C, 0x36, 0x7D, 0x00, 0x50, 0xAA,
    0xD5, 0x49, 0xFA, 0x7D, 0xDC, 0xC8, 0x5C, 0xE7
};
BLEUuid Wearable_UUID = BLEUuid(CUSTOM_UUID);

// e4a6f2a1-2967-4479-be73-f6f4b5cd07f8                this uuid is for the dispensers 
const uint8_t CUSTOM_UUID2[] =
{
    0xF8, 0x07, 0xCD, 0xB5, 0xF4, 0xF6, 0x73, 0xBE,
    0x79, 0x44, 0x67, 0x29, 0xA1, 0xF2, 0xA6, 0xE4
};
BLEUuid Dispenser_UUID = BLEUuid(CUSTOM_UUID2);

// OTA DFU service
BLEDfu bledfu;

// Add BLE services
BLEUart peripheral;       // uart over ble, as the peripheral

// Central UART client
BLEClientUart clientUart;   // uart over ble, as the central (so that peripherals can connect to each other)

char Wearable_ID[4+1] = "D001";     // D: doctor; N: nurse;

/* This struct is used to track detected nodes */
typedef struct node_record_s
{
  uint8_t  addr[6];    // Six byte device address
  int8_t   rssi;       // RSSI value
  uint32_t timestamp;  // Timestamp for invalidation purposes
  int8_t   reserved;   // Padding for word alignment
} node_record_t;

node_record_t records[ARRAY_SIZE];

node_record_t Reset_Scanned_Node(); //function for resetting nodes
void adv_stop_callback(void);


void setup() 
{
   
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb    open serial port to enable advertising
  delay(100);
  Serial.println("Wearable test");
  Serial.println("----------------------------------------\n");

  pinMode(feedback_pin, OUTPUT);
  digitalWrite(feedback_pin, LOW);

  /* Clear the results list */
  memset(records, 0, sizeof(records));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    records[i].rssi = -128;
  }

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  
  // Initialize Bluefruit with max concurrent connections as Peripheral = 1, Central = 1
  // SRAM usage required by SoftDevice will increase with number of connections
  Bluefruit.begin(1,1);
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName(Wearable_ID);            // room No. dispenser No.

  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(cent_connect_callback);
  Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(1000);

  // Configure and Start BLE Uart Service
  peripheral.begin();
  peripheral.setRxCallback(prph_bleuart_rx_callback);

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(cent_bleuart_rx_callback);

  // reset scanned_node array
  for (int i=0; i<ARRAY_SIZE; i++) 
    {
      records[i] = Reset_Scanned_Node();                   // reset record_nodes
    }

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
  Serial.println("Central scanning has started"); 


  // Set up and start advertising
  startAdv();

  Serial.println("Advertising has started"); 
}

void startAdv(void)
{   
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.ScanResponse.addUuid(Wearable_UUID);              // the scanresponse can be advertised when requested
  Bluefruit.ScanResponse.addName();                           // scanresponse can double the advertisiing package (62 bytes)
  // Bluefruit.Advertising.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms   can be modified
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.setStopCallback(adv_stop_callback);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(50, 100);          // in units of 0.625 ms, set advertising intervals, the first is fast mode, the second is slow mode
  Bluefruit.Advertising.setFastTimeout(1);           // number of seconds in fast mode   // try to not use the fast advertising mode
  Bluefruit.Advertising.start(0);                     // Stop advertising entirely after ADV_TIMEOUT seconds; 0 forever
}

// Peripheral functions
// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("Disconnected");
}

void prph_bleuart_rx_callback(uint16_t conn_handle)
{
  // receive data from dispenser
  // char str[2];
  // wearable.read(str, 2);                           // this needs to know the size

  // Serial.print("[Prph] RX: ");
  // Serial.println(str);  
                      
  String Waiting_BLEuart;                             // a good way to read string without knowing its size;
  while (peripheral.available()) 
  {                      
    uint8_t inChar = (uint8_t)peripheral.read();        // ble uart send uint8_t
    Waiting_BLEuart += (char)inChar;                  // uint8_t -> char -> string
    Serial.println(Waiting_BLEuart); 
  } 

  Serial.println(Waiting_BLEuart); 

  if (Waiting_BLEuart == "hands_washed") 
  {
    uint8_t write_buf[] = "rec";                 // rec: received
//uint8_t write_buf[10];// = "received";   

    peripheral.write(write_buf, sizeof(write_buf));     // received acknowledgement sent to dispenser
  }

  if (Waiting_BLEuart == "not_washed") 
  {
    uint8_t write_buf2[] = "rec";                  // rec: received
    peripheral.write(write_buf2, sizeof(write_buf2));     // received acknowledgement sent to dispenser

    // adding vibrating feedback
    Serial.println("Vibrating feedback");
    digitalWrite(feedback_pin, HIGH);
    delay(1000);
    digitalWrite(feedback_pin, LOW);
    
  }
}

void cent_connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));

  Serial.print("[Cent] Connected to ");
  Serial.println(peer_name);;

  if ( clientUart.discover(conn_handle) )
  {
    // Enable TXD's notify
    clientUart.enableTXD();
  }else
  {
    // disconnect since we couldn't find bleuart service
    Bluefruit.disconnect(conn_handle);
  }  
}

void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  
  Serial.println("[Cent] Disconnected");
}

void cent_bleuart_rx_callback(BLEClientUart& cent_uart)
{
  char str[20+1] = { 0 };
  cent_uart.read(str, 20);
      
  Serial.print("[Cent] RX: ");
  Serial.println(str);

  if ( peripheral.notifyEnabled() )
  {
    // Forward data from our peripheral to Mobile
    peripheral.print( str );
  }else
  {
    // response with no prph message
    clientUart.println("[Cent] Peripheral role not connected");
  }  
}

void scan_callback(ble_gap_evt_adv_report_t* report)
{
   node_record_t record;
  
  /* Prepare the record to try to insert it into the existing record list */
  memcpy(record.addr, report->peer_addr.addr, 6); /* Copy the 6-byte device ADDR */
  record.rssi = report->rssi;                     /* Copy the RSSI value */ //need to implement Kalman filter here
  record.timestamp = millis();                    /* Set the timestamp (approximate) */

  /* Attempt to insert the record into the list */
  //if (insertRecord(&record) == 1)                 /* Returns 1 if the list was updated */
  //{
    printRecordList();                            /* The list was updated, print the new values */
    Serial.println("");
  // Since we configure the scanner with filterUuid()
  // Scan callback only invoked for device with bleuart service advertised  
  // Connect to the device with bleuart service in advertising packet  
  Bluefruit.Central.connect(report);
  //}
}

/**
 * Callback invoked when advertising is stopped by timeout
 */ 
void adv_stop_callback(void)
{
  Serial.println("Advertising time passed, advertising will now stop.");
}

node_record_t Reset_Scanned_Node() //function for resetting nodes
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

void loop() 
{
  
}
