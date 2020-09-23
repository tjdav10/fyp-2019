
/*  This example scans for advertising devices (peripherals) in range,
 *  looking for a specific UUID in the advertising packet. When this
 *  UUID is found, it will display an alert, sorting devices detected
 *  in range by their RSSI value, which is an approximate indicator of
 *  proximity (though highly dependent on environmental obstacles, etc.).
 *  
 *  
 *  This example is intended to be used with multiple peripheral
 *  devices that are also running this program but with different names.
 *  
 *  TODO:
 *  Implement counter or RTC to measure duration of proximity - pseudocode written
 *    Duration counting doesn't really work without RTC as the millis() function is delayed - need the RTC to consistently count time
 *  
 *  Log date of interaction using RTC
 *  
 *  Increase number of available spots on list and compare memory size needed
 *  
 *  Implement final Kalman filter - pretty much done just need to test seperately then implement here
 *  
 *  Add battery level checking and transmission from batteryExampleCode.ino - done but need to test
 *  
 *  ARRAY_SIZE
 *  ----------
 *  The numbers of peripherals tracked can be set via the
 *  ARRAY_SIZE macro. Must be at least 2.
 *  
 *  TIMEOUT_MS - not needed as they will never be removed
 *  ----------
 *  This value determines the number of milliseconds before a tracked
 *  peripheral has it's last sample 'invalidated', such as when a device
 *  is tracked and then goes out of range.
 */

#include <string.h>
#include <bluefruit.h>
#include <ble_gap.h>
#include <SPI.h>
#include <stdio.h>
#include <math.h>

#define ARRAY_SIZE     (4)    // The number of RSSI values to store and compare
#define TIMEOUT_MS     (2500) // Number of milliseconds before a record is invalidated in the list


// Custom UUID used to differentiate this device.
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

/* This struct is used to track detected nodes */
typedef struct node_record_s
{
  uint8_t  addr[6];    // Six byte device address
  int8_t rssi; // current RSSI
  int8_t   filtered_rssi;       // min RSSI value
  uint32_t timestamp;  // Timestamp for invalidation purposes
  uint32_t first; // first timestamp
  uint32_t duration; //duration of contact (non RTC)
  char name[4];       // Name of detected device
  //int8_t   reserved;   // Padding for word alignment
  uint32_t count;
} node_record_t;

// Kalman struct for tracking parameters - make an array equal to ARRAY_SIZE and populate it with kalman structs - reset the array when list is reset as well
typedef struct kalman {
  // Parameters
  float meas_uncertainty;
  float est_uncertainty;
  float q;
  // Variables
  float prev_est;
  float cur_est;
  float kal_gain;
  int8_t filtered;
  int8_t raw;
} kal;

// 32 to be replaced when RTC arrives
struct date_time {
  char date[32];
  char first_time_70[32];
  char time_left_70[32];
};


node_record_t records[ARRAY_SIZE];

node_record_t test_list[ARRAY_SIZE];

// Creating array of kalman filter objects
kal kalmans[ARRAY_SIZE];




// Battery level variables
int adcin    = A6;  // A6 == VDIV is for battery voltage monitoring 
int adcvalue = 0;
float mv_per_lsb = 3600.0F/4096.0F; // 12-bit ADC with 3.6V input range
                                    //the default analog reference voltage is 3.6 V
float v_max = 4.00; // the maximum battery voltage
float v_min = 3.00; // min battery voltage

// Add BLE services
BLEUart wearable;       // uart over ble, as the peripheral
char id[4+1] = "N001";

void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("Contact tracing proximity app");
  Serial.println("-------------------------------------\n");

  analogReadResolution(12); // Can be 8, 10, 12 or 14

  /* Clear the results list */
  memset(records, 0, sizeof(records));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    records[i].rssi = -128;
  }

  /* Clear the test_list list */
  memset(records, 0, sizeof(test_list));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value (-128) for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    test_list[i].rssi = 60; // changed to 60 for debugging
  }

  // Setting up parameters for kalman filter
  for(int i=0; i<ARRAY_SIZE; i++)
  {
    setUpKalman(&kalmans[i]);
  }
  
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  /* Enable both peripheral and central modes */
  if ( !Bluefruit.begin(1, 1) )
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
    Serial.println("Bluefruit initialized (central mode)");
  }
  
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values (+4)

  /* Set the device name */
  Bluefruit.setName(id);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(250);

  // Configure and Start BLE Uart Service
  wearable.begin();
  wearable.setRxCallback(prph_bleuart_rx_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Filter out packet with a min rssi
   * - Interval = 100 ms, window = 50 ms
   * - Use active scan (used to retrieve the optional scan response adv packet)
   * - Start(0) = will scan forever since no timeout is given
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  //Bluefruit.Scanner.filterRssi(-80);            // Only invoke callback for devices with RSSI >= -80 dBm
  Bluefruit.Scanner.filterUuid(uuid);           // Only invoke callback if the target UUID was found
  Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
  Bluefruit.Scanner.useActiveScan(true);        // Request scan response data MAYBE CHANGE THIS TO FALSE / OR MAKE IT SO THAT NAME IS TAKEN FROM SCAN RESPONSE
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds
  Serial.println("Scanning ...");

  // Set up and start advertising
  startAdv();
  Serial.println("Advertising ...");

  // Setting up test list for transmission (testing)
  memcpy(test_list[0].name, "D001", 4); // 
  test_list[0].count = 5000;
  memcpy(test_list[1].name, "D002", 4); // 
  test_list[1].count = 5000;
  memcpy(test_list[2].name, "D003", 4); // 
  test_list[2].count = 5000;
  memcpy(test_list[3].name, "D004", 4); // 
  test_list[3].count = 5000;
  
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

  // if there is not enough room in the advertising packet for name
  // ,store it in the Scan Response instead
  //Bluefruit.ScanResponse.addName();

  // Can actually have short name in adv packet, 5 characters max
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
  Bluefruit.Advertising.setInterval(32, 244);    // in units of 0.625 ms (152.5ms = 244*0.625) (probably a bit too fast)
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start();
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char peer_name[32] = { 0 };
  connection->getPeerName(peer_name, sizeof(peer_name));

  Serial.print("Connected to ");
  Serial.println(peer_name);
  
  uint8_t buf[4]; // for copying name
  char str[32]; // for converting int8_t to char array for sending over BLE
  delay(1000); // delay for debugging on phone app - must be present for actual system but doesn't need to be as big
  // Sending list over BLE (works)
//  for (int i=0; i<ARRAY_SIZE; i++)
//  {
//    if(test_list[1].name[1] != 0) // if name first char is non-zero value, it sends the list so blank entries are not sent (confirmed working)
//    {
//      //This combines all fields from the record into a single string for transmission
//      sprintf(str, "%s %.4s", id, test_list[i].name); // maximum of 20 chars
//      wearable.write(str); // write str
//    }
//  }
  
  memset(str, 0, sizeof(str)); // clear the str buffer
  
  // Sending battery information
  adcvalue = analogRead(adcin);
  sprintf(str, "B %.4s %.2f %.2f %.2f", id, (adcvalue * mv_per_lsb/1000*2), v_max, v_min); // B at start to indicate battery level, limit id to 4 chars, and voltages to 2 decimal places
  wearable.write(str);

  memset(str, 0, sizeof(str)); // clear the str buffer
  //Bluefruit.disconnect(conn_handle); // disconnects once list is sent
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("Disconnected");
  //list_sent = false;
}

void prph_bleuart_rx_callback(uint16_t conn_handle)
{
  (void) conn_handle;
}

/* This callback handler is fired every time a valid advertising packet is detected */
void scan_callback(ble_gap_evt_adv_report_t* report)
{  
  node_record_t record;
  uint8_t buffer[4]; // used for names of 4 chars long
  
  /* Prepare the record to try to insert it into the existing record list */
  memcpy(record.addr, report->peer_addr.addr, 6); /* Copy the 6-byte device ADDR */
  record.rssi = report->rssi;                     /* Copy the RSSI value */
  Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)); // puts name of device in buffer
  memcpy(record.name, buffer, 4); // copy contents of buffer to name field
  memset(buffer, 0, sizeof(buffer)); // reset buffer
  record.timestamp = millis();                    /* Set the timestamp (approximate) */
  /* Attempt to insert the record into the list */
  if (insertRecord(&record) == 1)                 /* Returns 1 if the list was updated */
  {
    printRecordList();                            /* The list was updated, print the new values */
  }

  // For Softdevice v6: after received a report, scanner will be paused
  // We need to call Scanner resume() to continue scanning
  Bluefruit.Scanner.resume();
}

/* Prints the current record list to the Serial Monitor */
void printRecordList(void)
{
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    Serial.printf("[%i] ", i);
    //Serial.printBuffer(records[i].addr, 6, ':');
    Serial.printf("%.4s ",records[i].name);
    Serial.printf("%i (%u ms)\n", records[i].filtered_rssi, records[i].duration);
  }
}


/*  This function will check if any records in the list
 *  have expired and need to be invalidated, such as when
 *  a device goes out of range.
 *  
 *  Returns the number of invalidated records, or 0 if
 *  nothing was changed.
 */
int invalidateRecords(void) // this doesn't do anything anymore but keeping it in so rest of program works
{
/*
  uint8_t i;
  uint8_t buffer[4+1]; // used for names of 4 chars long
  int match = 0;


  if (millis() <= TIMEOUT_MS)
  {
    return 0;
  }


  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (records[i].timestamp) // Ignore zero"ed records
    {
      if (millis() - records[i].timestamp >= TIMEOUT_MS)
      {

        memset(&records[i], 0, sizeof(node_record_t));
        records[i].rssi = -128;
        match++;
      }
    }
  }



  if (match)
  {
    // Serial.printf("Invalidated %i records!\n", match);    
  }

*/
  //return match;
  return 0;
}

/* This function attempts to insert the record if there is room */
/* Returns 1 if a change was made, otherwise 0 */
int insertRecord(node_record_t *record)
{
  uint8_t i;
  
  /* Invalidate results older than n milliseconds */
  //invalidateRecords();
  

  /*    Check for a match on existing device address */
  /*    Replace it if a match is found */
  uint8_t match = 0;
  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (memcmp(records[i].addr, record->addr, 6) == 0)
    {
      match = 1;
    }
    if (match)
    {
      // Update raw RSSI then filter
      updateRaw(&kalmans[i], record->rssi);
      filter(&kalmans[i]);
      record->filtered_rssi = kalmans[i].filtered;

      void updateDuration();

      // If previous -70 timestamp is earlier than current one, keep the old (OR MIGHT JUST KEEP THE ORIGINAL NO MATTER WHAT?)
      // Or if formatted as string can do strcmp just to check difference. Will depend on output from RTC
      /*pseudo code
       * if (records[i].timestamp < record->timestamp)
       *  {
       *    record->timestamp = records[i].timestamp  //(or do memcpy)
       *  }
       * 
       * 
       */
            
      memcpy(&records[i], record, sizeof(node_record_t));
    
      
      goto sort_then_exit;
    }
  }

  /*    Check for zero'ed records */
  /*    Insert if a zero record is found */
  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (records[i].name[0] == 0)
    {
      // Update raw RSSI then filter
      updateRaw(&kalmans[i], record->rssi);
      filter(&kalmans[i]);
      record->filtered_rssi = kalmans[i].filtered;
      
      memcpy(&records[i], record, sizeof(node_record_t));
      
      goto sort_then_exit;
    }
  }


  // Nothing to do ... list must be full and this interaction is missed :(
  return 0;

sort_then_exit:
  return 1; // returning 1 means list has been updated
}

// Kalman filter function
void filter(kal *k) // pass by reference to save memory
{
  k->kal_gain = k->est_uncertainty/(k->est_uncertainty + k->meas_uncertainty); // compute kalman gain
  k->cur_est = k->prev_est + k->kal_gain*(k->raw - k->prev_est); // compute estimate for current time step
  k->est_uncertainty = (1 - k->kal_gain)*k->est_uncertainty + abs(k->prev_est - k->cur_est)*k->q; // update estimate uncertainty
  k->prev_est = k->cur_est; // update previous estimate for next loop iteration
  k->filtered = k->cur_est;
}

void setUpKalman(kal *k) // parameters need to be updated
{
  k->meas_uncertainty = 1;
  k->est_uncertainty = 1;
  k->q = 1;
}

void updateRaw(kal *k, uint8_t rssi)
{
  k->raw = rssi;
}

void updateDuration(node_record_t *record)
{
  uint32_t now = millis();
  record->duration = abs(now - record->timestamp);
}

void loop() 
{
  // Forward from Serial to BLEUART
  if (Serial.available())
  {
    // Delay to get enough input data since we have a
    // limited amount of space in the transmit buffer
    delay(20);
 
    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf));
    wearable.write( buf, count );
  }
 
  // Forward from BLEUART to Serial
  if ( wearable.available() )
  {
    uint8_t ch;
    ch = (uint8_t) wearable.read();
    Serial.write(ch);
  }

   /* Invalidate old results once per second in addition
   * to the invalidation in the callback handler. */
  /* ToDo: Update to use a mutex or semaphore since this
   * can lead to list corruption as-is if the scann results
   * callback is fired in the middle of the invalidation
   * function. */
  //if (invalidateRecords())
  {
    /* The list was updated, print the new values */
    //printRecordList();
    //Serial.println("");
  }
}
