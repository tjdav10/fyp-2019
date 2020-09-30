/* BLE Peripheral Program for RSSI data collection
 * ------------------------------------------------
 * This program is used as the peripheral for collecting RSSI values for Kalman filter design.
 * 
 * The parameters that are changed are the advertising interval and TX power.
 * 
 * Bluefruit.setTxPower(4) changes the TX power.
 * 
 * Bluefruit.Advertising.setInterval(800, 3200)    // in units of 0.625 ms
 *  - changes the advertising interval. First argument is fast mode, second is slow mode
 *  
 *  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
 */

#include <bluefruit.h>
#include <ble_gap.h>

// Software Timer for blinking RED LED
SoftwareTimer blinkTimer;

// Custom UUID used to differntiate this device.
// Use any online UUID generator to generate a valid UUID.
// Note that the byte order is reversed ... CUSTOM_UUID
// below corresponds to the follow value:
// df67ff1a-718f-11e7-8cf7-a6006ad3dba0
const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};

BLEUuid uuid = BLEUuid(CUSTOM_UUID);

void setup() 
{
  //Serial.begin(115200);
  //while ( !Serial ) delay(10);   // for nrf52840 with native usb

  // Initialize blinkTimer for 1000 ms and start it
  blinkTimer.begin(1000, blink_timer_callback);
  blinkTimer.start();

  if (!Bluefruit.begin())
  {
    Serial.println("Unable to init Bluefruit");
    while(1)
    {
      digitalToggle(LED_RED);
      delay(100);
    }
  }
  else
  {
    Serial.println("Bluefruit initialized (peripheral mode)");
  }

  Bluefruit.setTxPower(-8);    // Check bluefruit.h for supported values
  Bluefruit.setName("D005");

  // Set up and start advertising
  startAdv();

  Serial.println("Advertising started"); 
}

void startAdv(void)
{   
  // Note: The entire advertising packet is limited to 31 bytes!
  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Preferred Solution: Add a custom UUID to the advertising payload, which
  // we will look for on the Central side via Bluefruit.Scanner.filterUuid(uuid);
  // A valid 128-bit UUID can be generated online with almost no chance of conflict
  // with another device or etup
  Bluefruit.Advertising.addUuid(uuid);
  Bluefruit.Advertising.addName();

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
//  Bluefruit.Advertising.setInterval(800, 3200);    // in units of 0.625 ms
  Bluefruit.Advertising.setInterval(800, 800);    // in units of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(0);      // number of seconds in fast mode
  Bluefruit.Advertising.start();
}

void loop() 
{
}

/**
 * Software Timer callback is invoked via a built-in FreeRTOS thread with
 * minimal stack size. Therefore it should be as simple as possible. If
 * a periodically heavy task is needed, please use Scheduler.startLoop() to
 * create a dedicated task for it.
 * 
 * More information http://www.freertos.org/RTOS-software-timer.html
 */
void blink_timer_callback(TimerHandle_t xTimerID)
{
  (void) xTimerID;
  digitalToggle(LED_RED);
}
