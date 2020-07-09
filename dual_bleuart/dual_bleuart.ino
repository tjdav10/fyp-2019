/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
 * This sketch demonstrate how to run both Central and Peripheral roles
 * at the same time. It will act as a relay between an central (mobile)
 * to another peripheral using bleuart service.
 * 
 * Mobile <--> DualRole <--> peripheral Ble Uart
 */
#include <bluefruit.h>

#define ARRAY_SIZE (4) //number of records to keep track of
// OTA DFU service
BLEDfu bledfu;

// Peripheral uart service
BLEUart bleuart;

// Central uart client
BLEClientUart clientUart;


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


void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb


  /* Clear the results list */
  memset(records, 0, sizeof(records));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    records[i].rssi = -128;
  }

  
  Serial.println("Bluefruit52 Dual Role BLEUART Example");
  Serial.println("-------------------------------------\n");
  
  // Initialize Bluefruit with max concurrent connections as Peripheral = 1, Central = 1
  // SRAM usage required by SoftDevice will increase with number of connections
  Bluefruit.begin(1, 1);
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName(Wearable_ID);

  // Callbacks for Peripheral
  Bluefruit.Periph.setConnectCallback(prph_connect_callback);
  Bluefruit.Periph.setDisconnectCallback(prph_disconnect_callback);

  // Callbacks for Central
  Bluefruit.Central.setConnectCallback(cent_connect_callback);
  Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();
  bleuart.setRxCallback(prph_bleuart_rx_callback);

  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(cent_bleuart_rx_callback);


  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Filter only accept bleuart service
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.filterUuid(Wearable_UUID);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void loop()
{
  // do nothing, all the work is done in callback
}

/*------------------------------------------------------------------*/
/* Peripheral
 *------------------------------------------------------------------*/
void prph_connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));

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

void prph_bleuart_rx_callback(uint16_t conn_handle)
{
  (void) conn_handle;
  
  // Forward data from Mobile to our peripheral
  char str[20+1] = { 0 };
  bleuart.read(str, 20);

  Serial.print("[Prph] RX: ");
  Serial.println(str);  

  if ( clientUart.discovered() )
  {
    clientUart.print(str);
  }else
  {
    bleuart.println("[Prph] Central role not connected");
  }
}

/*------------------------------------------------------------------*/
/* Central
 *------------------------------------------------------------------*/
void scan_callback(ble_gap_evt_adv_report_t* report)
{
   node_record_t record;
  
  /* Prepare the record to try to insert it into the existing record list */
  memcpy(record.addr, report->peer_addr.addr, 6); /* Copy the 6-byte device ADDR */
  record.rssi = report->rssi;                     /* Copy the RSSI value */ //need to implement Kalman filter here
  record.timestamp = millis();                    /* Set the timestamp (approximate) */

  /* Attempt to insert the record into the list */
  if (insertRecord(&record) == 1)                 /* Returns 1 if the list was updated */
  {
    printRecordList();                            /* The list was updated, print the new values */
    Serial.println("");
  // Since we configure the scanner with filterUuid()
  // Scan callback only invoked for device with bleuart service advertised  
  // Connect to the device with bleuart service in advertising packet  
  Bluefruit.Central.connect(report);
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

/**
 * Callback invoked when uart received data
 * @param cent_uart Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void cent_bleuart_rx_callback(BLEClientUart& cent_uart)
{
  char str[20+1] = { 0 };
  cent_uart.read(str, 20);
      
  Serial.print("[Cent] RX: ");
  Serial.println(str);

  if ( bleuart.notifyEnabled() )
  {
    // Forward data from our peripheral to Mobile
    bleuart.print( str );
  }else
  {
    // response with no prph message
    clientUart.println("[Cent] Peripheral role not connected");
  }  
}
