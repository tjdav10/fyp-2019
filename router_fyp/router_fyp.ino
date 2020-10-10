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

char ID[4+1] = "R001";

BLEClientUart clientUart; // bleuart client

const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};

BLEUuid uuid = BLEUuid(CUSTOM_UUID);

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
  Serial.begin(9600);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
//  Serial.println("starting...");
  
  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
  
  Bluefruit.setName(ID);

//  analogReadResolution(12); // Can be 8, 10, 12 or 14
 
 
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
  Bluefruit.Scanner.filterUuid(uuid);
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
  // Need to have some condition here to see if recently connected to this device (maybe a list of recent names)
  if(1)
  {
 
    // Connect to device with bleuart service in advertising
    Bluefruit.Central.connect(report);
  }else
  {      
    // For Softdevice v6: after received a report, scanner will be paused
    // We need to call Scanner resume() to continue scanning
    Bluefruit.Scanner.resume();
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
  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));
//  Serial.print("Connected to ");
//  Serial.println(peer_name);
  
  char str[32];
  if ( clientUart.discover(conn_handle) )
  {
    clientUart.enableTXD();

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
  char received_string[19+1];
  while ( uart_svc.available() )
  {
    uart_svc.read(received_string, sizeof(received_string)-1);
    //Serial.print( (char) uart_svc.read() );
//    if(received_string[0] == 'B' || received_string[0] == 'N' || received_string[0] == 'D' || received_string[0] == 'V')
    {
      Serial.print(received_string);
      
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
