/* Use this for developing kalman filter/tuning
 *  Log:
 *  It seems like variance of the measurement noise is too high - it means filter response is too slow
 * 
 * 
 */


#include <bluefruit.h>
#include <math.h>

const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};
BLEUuid uuid = BLEUuid(CUSTOM_UUID);

// Kalman struct for tracking parameters - make an array equal to ARRAY_SIZE and populate it with kalman structs - reset the array when list is sent as well
typedef struct kalman {
  // Parameters
  float meas_uncertainty;
  float est_uncertainty;
  float q;
  // Variables
  float prev_est;
  float cur_est;
  float kal_gain;
  float out;
  int8_t meas;
} kal;

kal test;

void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("raw kalman");

  
  test.meas_uncertainty = 1;
  test.est_uncertainty =  test.meas_uncertainty;
  test.q = 0.008;

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this LED manually
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("Bluefruit52");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);  
  

  // Set up and start advertising
  startAdv();

  //Serial.println("Please use Adafruit's Bluefruit LE app to connect");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include custom UUID
  Bluefruit.Advertising.addUuid(uuid);

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
  if ( Bluefruit.connected() )
  {
    uint16_t conn_hdl = Bluefruit.connHandle();

    // Get the reference to current connected connection 
    BLEConnection* connection = Bluefruit.Connection(conn_hdl);

    // get the RSSI value of this connection
    // monitorRssi() must be called previously (in connect callback)
    int8_t rssi = connection->getRssi();

    test.meas = rssi;
    filter(&test);
    
    
    
    //Serial.printf("%d %f", rssi, test.out);
    Serial.printf("%d %.0f", rssi, test.out); // rounded
    Serial.println();

    // print onece every 30 ms (approx the speed packets are advertised in slow mode)
    delay(30);
  }
}

void connect_callback(uint16_t conn_handle)
{

  // Get the reference to current connection
  BLEConnection* conn = Bluefruit.Connection(conn_handle);

  // Start monitoring rssi of this connection
  // This function should be called in connect callback
  // no parameters means we don't use rssi changed callback 
  conn->monitorRssi();
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

}

void filter(kal *k) // pass by reference to save memory
{
  k->kal_gain = k->est_uncertainty/(k->est_uncertainty + k->meas_uncertainty); // compute kalman gain
  k->cur_est = k->prev_est + k->kal_gain*(k->meas - k->prev_est); // compute estimate for current time step
  k->est_uncertainty = (1 - k->kal_gain)*k->est_uncertainty + abs(k->prev_est - k->cur_est)*k->q; // update estimate uncertainty
  k->prev_est = k->cur_est; // update previous estimate for next loop iteration
  k->out = k->cur_est;
}
