#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// BLE Service
BLEUart bleuart; // uart over ble

int adcin    = A6;  // A6 == VDIV is for battery voltage monitoring 
int adcvalue = 0;
float mv_per_lsb = 3600.0F/4096.0F; // 12-bit ADC with 3.6V input range
                                    //the default analog reference voltage is 3.6 V
char buffer[10];

void setup()
{
  //Serial.begin(115200);

  
  //Serial.println("Bluefruit52 BLEUART Example");
  //Serial.println("---------------------------\n");

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("Bluefruit52");
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start BLE Uart Service
  bleuart.begin();



  // Set up and start advertising
  startAdv();

  //Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode");
  //Serial.println("Once connected, enter character(s) that you wish to send");

  analogReadResolution(12); // Can be 8, 10, 12 or 14
  delay(5);
  //Serial.begin(115200);
  //while ( !Serial ) delay(10);   // for nrf52840 with native usb
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
  // Forward data from HW Serial to BLEUART
  while (Serial.available())
  {
//    // Delay to wait for enough input, since we have a limited transmission buffer
//    delay(2);
//
//    uint8_t buf[64];
//    int count = Serial.readBytes(buf, sizeof(buf));
//    bleuart.write( buf, count );
  }

  // Forward from BLEUART to HW Serial
  while ( bleuart.available() )
  {
//    uint8_t ch;
//    ch = (uint8_t) bleuart.read();
//    Serial.write(ch);
  }
  adcvalue = analogRead(adcin);
  sprintf(buffer, "%.2f volts", (adcvalue * mv_per_lsb/1000*2));
//  Serial.print("ADC: ");
//  Serial.print(adcvalue);
//  Serial.print(";  Battery voltage: ");
//  Serial.println((float)adcvalue * mv_per_lsb/1000*2);
  bleuart.write(buffer);
  memset(buffer, 0, sizeof(buffer));
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

//  Serial.print("Connected to ");
//  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

//  Serial.println();
//  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}
