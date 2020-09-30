/* BLE Central Program for RSSI data collection
 * ------------------------------------------------
 * This program is used as the central for collecting RSSI values for Kalman filter design.
 * 
 * The parameters that are changed are the scanning interval.
 * 
 * Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms - (interval = 100ms, window = 50ms)
 * 
 * 
 */

#include <bluefruit.h>

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

//  Serial.println("Bluefruit52 Central Scan Example");
//  Serial.println("--------------------------------\n");

  // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
  // SRAM usage required by SoftDevice will increase dramatically with number of connections
  Bluefruit.begin(0, 1);
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("Bluefruit52");

  // Start Central Scan
  Bluefruit.setConnLedInterval(250);
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.filterUuid(uuid);
  Bluefruit.Scanner.setInterval(160, 160);       // in units of 0.625 ms - (interval = 100ms, window = 50ms)
  Bluefruit.Scanner.start(0);

  //Serial.println("Scanning ...");
}

void scan_callback(ble_gap_evt_adv_report_t* report)
{
  //Serial.println("Timestamp Addr              Rssi Data");

  //Serial.printf("%09d ", millis());
  
  // MAC is in little endian --> print reverse
  //Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  //Serial.print(" ");

    Serial.printf("%d %i\n", (millis()),report->rssi);
//    Serial.printf("%i\n", report->rssi);

  //Serial.printBuffer(report->data.p_data, report->data.len, '-');
  //Serial.println();

  // Check if advertising contain BleUart service
  if ( Bluefruit.Scanner.checkReportForUuid(report, BLEUART_UUID_SERVICE) )
  {
    //Serial.println("                       BLE UART service detected");
  }

  //Serial.println();

  // For Softdevice v6: after received a report, scanner will be paused
  // We need to call Scanner resume() to continue scanning
  Bluefruit.Scanner.resume();
}

void loop() 
{
  // nothing to do
}
