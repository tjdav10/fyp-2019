/* A simple Central BLE Uart program to forward data received from wearables to Serial (to raspberry pi)
 * TODO:
 * Get name of last connected wearable
 *  Make it so it won't auto-reconnect to the last wearable
 * 
 * 
 * 
 */
#include <bluefruit.h>

BLEClientUart clientUart; // bleuart client

char last_conn[4+1];

const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};

BLEUuid uuid = BLEUuid(CUSTOM_UUID);

void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
 
  Serial.println("Dispenser Central BLEUART");
  Serial.println("-----------------------------------\n");
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
  
  Bluefruit.setName("R001D001");
 
  // Init BLE Central Uart Serivce
  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);
 
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
  Bluefruit.Scanner.filterUuid(uuid);           // Only invoke callback if the target UUID was found
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
  
  // UUID is already filtered so just need to check ID of last wearable connected to

  uint8_t buffer[4]; // used for names of 4 chars long
  char name[4];
  char str[4];
  Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)); // puts name of device in buffer
  memcpy(name, buffer, 4);
  sprintf(str, "%s", name);
  Serial.printf("strcmp returned %i\n", strcmp(str,last_conn));
  Serial.printf("str = %s\n", str);
  Serial.printf("last_conn = %s\n", last_conn);
  Serial.println();
  if(strcmp(name, last_conn)!=0) // if name and last_conn are different, connect to the device
  {
    Bluefruit.Central.connect(report);
  }

  else
  {
    Bluefruit.Scanner.resume(); // if recently connected to, start scanning again
  }
}
 
/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char peer_name[4];

  Serial.println("Connected");
 
  //Serial.print("Discovering BLE Uart Service ... ");
  {
    //Serial.println("Found it");
    
    //Serial.println("Enable TXD's notify");
    clientUart.enableTXD();
 
    //Serial.println("Ready to receive from peripheral");
  }
  {
    
    // disconnect since we couldn't find bleuart service
    //Bluefruit.disconnect(conn_handle);
  }  
  connection->getPeerName(last_conn, sizeof(last_conn)); // may need to increase size of last_conn (currently char array length 5)
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
  
  //Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
 
/**
 * Callback invoked when uart received data
 * @param uart_svc Reference object to the service where the data 
 * arrived. In this example it is clientUart
 */
void bleuart_rx_callback(BLEClientUart& uart_svc)
{
  //Serial.print("[RX]: ");
  
  while ( uart_svc.available() )
  {
    Serial.print( (char) uart_svc.read() );
  }
 
  //Serial.println();
}
 
void loop()
{
  if ( Bluefruit.Central.connected() )
  {
    // Not discovered yet
    if ( clientUart.discovered() )
    {
      // Discovered means in working state
      // Following code is for Serial->BLE forwarding
      // Get Serial input and send to Peripheral
      if ( Serial.available() )
      {
        delay(2); // delay a bit for all characters to arrive
        
        char str[20+1] = { 0 };
        Serial.readBytes(str, 20);
        
        clientUart.print( str );
      }
    }
  }
}
