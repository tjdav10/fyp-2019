#include <bluefruit.h>

#define feedback_pin 9

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

// Add BLE services
BLEUart wearable;       // uart over ble, as the peripheral

char Wearable_ID[4+1] = "D001";     // D: doctor; N: nurse;

void setup() 
{
   
//  Serial.begin(115200);
//  while ( !Serial ) delay(10);   // for nrf52840 with native usb    open serial port to enable advertising
//  delay(100);
//  Serial.println("Wearable test");
//  Serial.println("----------------------------------------\n");

  pinMode(feedback_pin, OUTPUT);
  digitalWrite(feedback_pin, LOW);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName(Wearable_ID);            // room No. dispenser No.

  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.setConnectCallback(connect_callback);
  Bluefruit.setDisconnectCallback(disconnect_callback);

  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(1000);

  // Configure and Start BLE Uart Service
  wearable.begin();
  wearable.setRxCallback(prph_bleuart_rx_callback);

  // Set up and start advertising
  startAdv();

  Serial.println("Advertising is started"); 
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

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  char central_name[32] = { 0 };
  Bluefruit.Gap.getPeerName(conn_handle, central_name, sizeof(central_name));

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

void prph_bleuart_rx_callback(void)
{
  // receive data from dispenser
  // char str[2];
  // wearable.read(str, 2);                           // this needs to know the size

  // Serial.print("[Prph] RX: ");
  // Serial.println(str);  
                      
  String Waiting_BLEuart;                             // a good way to read string without knowing its size;
  while (wearable.available()) 
  {                      
    uint8_t inChar = (uint8_t)wearable.read();        // ble uart send uint8_t
    Waiting_BLEuart += (char)inChar;                  // uint8_t -> char -> string
  } 

  Serial.println(Waiting_BLEuart); 

  if (Waiting_BLEuart == "hands_washed") 
  {
    uint8_t write_buf[] = "rec";                 // rec: received
//uint8_t write_buf[10];// = "received";   

    wearable.write(write_buf, sizeof(write_buf));     // received acknowledgement sent to dispenser
  }

  if (Waiting_BLEuart == "not_washed") 
  {
    uint8_t write_buf2[] = "rec";                  // rec: received
    wearable.write(write_buf2, sizeof(write_buf2));     // received acknowledgement sent to dispenser

    // adding vibrating feedback
    Serial.println("Vibrating feedback");
    digitalWrite(feedback_pin, HIGH);
    delay(1000);
    digitalWrite(feedback_pin, LOW);
    
  }
}

/**
 * Callback invoked when advertising is stopped by timeout
 */ 
void adv_stop_callback(void)
{
  Serial.println("Advertising time passed, advertising will now stop.");
}

void loop() 
{
  
}
