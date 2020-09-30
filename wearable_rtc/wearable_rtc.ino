
/*  CONTACT TRACING BLE PROGRAM FOR USE IN HAND HYGIENE PROJECT
 *  -----------------------------------------------------------
 *   v1:
 *   This program scans for advertising devices (peripherals) in range,
 *  looking for a specific UUID in the advertising packet. When this
 *  UUID is found, it will add a record to the list. Kalman filtering
 *  is performed on the RSSI to improve the accuracy of the measurement.
 *  A record consists of the name, time of first interaction, duration
 *  of interaction, raw rssi and filtered rssi. Name, duration of
 *  interaction, time of interaction are sent over BLE to a connecting
 *  dispenser. Once the list of records has been sent, it will not send again
 *  until 5 minutes has passed to allow time for interactions to be logged.
 *  
 *  The battery level is also sent when the dispenser connects to the wearable.
 *  
 *  Format of interaction record:
 * ------------------------------
 *  <name of this device> <name of other device> <number of seconds duration> <time of first contact>
 *  
 *  Format of battery level data:
 * ------------------------------
 * B <name of this device> <battery measured voltage> <max. battery voltage> <min battery voltage>
 *  
 *  
 *  This example is intended to be used with multiple peripheral
 *  devices that are also running this program but with different names.
 *  
 *  TODO:
 *  Duration of proximity works
 *    BUGS:
 *      the first duration in each record is a really big number for some reason - but normal after that (2779096485 is the number)
 *  
 *  Increase number of available spots on list and compare memory size needed
 *  
 *  Implement final Kalman filter - pretty much done just need to test seperately then implement here
 *  
 *  Write better quality code and refine comments
 *  
 *  Add thresholding code for adding records
 *    - WIP
 *    - Problem is that it keeps creating new records if RSSI is below threshold. Need to somehow discard first ~10s of RSSI data
 *  
 *  
 *  ARRAY_SIZE
 *  ----------
 *  The numbers of peripherals tracked can be set via the
 *  ARRAY_SIZE macro. Must be at least 2.
 *  
 *  TIMEOUT
 *  ----------
 *  This value determines the number of seconds before a tracked
 *  peripheral has the interaction recognised as ended. If the peripheral enters
 *  proximity again, a new record will be created
 *  
 *  RSSI_THRESHOLD
 *  ----------
 *  The RSSI threshold that needs to be crossed for a record to be created.
 */

#include <string.h>
#include <bluefruit.h>
#include <ble_gap.h>
#include <SPI.h>
#include <stdio.h>
#include <math.h>
#include "RTClib.h"

#define ID ("N001") // ID of this device
#define ARRAY_SIZE     (8)    // The number of RSSI values to store and compare
#define TIMEOUT     (60) // Number of seconds before a record is deemed complete, and seperate interaction will be logged
#define RSSI_THRESHOLD (-70)  // RSSI threshold to log interaction
#define CONVERGENCE_TIME (15) // kalman filter convergence time


RTC_PCF8523 rtc;

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
  uint32_t first; // first timestamp (unix time)
  uint32_t last; // last timestamp (unix time) - when moved out of proximity
  uint32_t duration; //duration of contact in seconds
  char name[4];       // Name of detected device
  uint8_t hour;
  uint8_t minute;
  char time[5];
  //int8_t   reserved;   // Padding for word alignment
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

uint32_t last_sent = 0; // used to store the last unix time the list was sent to dispenser

node_record_t records[ARRAY_SIZE];

node_record_t prelim[ARRAY_SIZE]; // preliminary storage of records to discard first 15s of filtered RSSI

// Creating array of kalman filter objects
kal kalmans[ARRAY_SIZE];

// Battery level variables
int adcin    = A6;  // A6 == VDIV is for battery voltage monitoring 
int adcvalue = 0;
float mv_per_lsb = 3600.0F/4096.0F; // 12-bit ADC with 3.6V input range
                                    //the default analog reference voltage is 3.6 V
float v_max = 4.06; // the maximum battery voltage
float v_min = 3.00; // min battery voltage

// Add BLE services
BLEUart wearable;       // uart over ble, as the peripheral
//char id[4+1] = "N001";

void setup()
{
  Serial.begin(115200); // remove this for final
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  while ( !Serial ) delay(10);   // for nrf52840 with native usb -  remove for final

  Serial.println("Contact tracing proximity app");
  Serial.println("-------------------------------------\n");

  rtc.start();

  analogReadResolution(12); // Can be 8, 10, 12 or 14

  /* Clear the results list */
  memset(records, 0, sizeof(records));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    records[i].rssi = -128;
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
    Serial.println("Bluefruit initialized");
  }
  
  Bluefruit.setTxPower(-8);    // Check bluefruit.h for supported values -

  /* Set the device name */
  Bluefruit.setName(ID);

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
   * - Interval = 100 ms, window = 100 ms
   * - Use active scan (used to retrieve the optional scan response adv packet)
   * - Start(0) = will scan forever since no timeout is given
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  //Bluefruit.Scanner.filterRssi(-80);            // Only invoke callback for devices with RSSI >= -80 dBm
  Bluefruit.Scanner.filterUuid(uuid);           // Only invoke callback if the target UUID was found
  Bluefruit.Scanner.setInterval(160, 160);       // in units of 0.625 ms
  Bluefruit.Scanner.useActiveScan(true);        // Request scan response data MAYBE CHANGE THIS TO FALSE / OR MAKE IT SO THAT NAME IS TAKEN FROM SCAN RESPONSE
  Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds
  Serial.println("Scanning ...");

  // Set up and start advertising
  startAdv();
  Serial.println("Advertising ..."); 
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
  Bluefruit.Advertising.setInterval(800, 800);    // in units of 0.625 ms (152.5ms = 244*0.625) (probably a bit too fast)
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
  
  char str[32]; // for converting int8_t to char array for sending over BLE
  DateTime now = rtc.now();
  
  if((((now.unixtime() - last_sent) > 300)) || last_sent==0)
  {
    delay(3000); // delay for debugging on phone app - must be present for actual system but doesn't need to be as big
    // Sending list over BLE (works) (maximum of 20 chars including spaces)
    for (int i=0; i<ARRAY_SIZE; i++)
    {
      if(records[i].name[1] != 0) // if name first char is non-zero value, it sends the list so blank entries are not sent (confirmed working)
      {
        //This combines all fields from the record into a single string for transmission
        sprintf(str, "%s %.4s %u %u:%02u", ID, records[i].name, records[i].duration, records[i].hour, records[i].minute); // maximum of 20 chars (X999 X999 9999 HH:MM)
        wearable.write(str); // write str
      }
    }
    
    memset(str, 0, sizeof(str)); // clear the str buffer
    
    // Sending battery information
    adcvalue = analogRead(adcin);
    sprintf(str, "B %.4s %.2f %.2f %.2f", ID, (adcvalue * mv_per_lsb/1000*2), v_max, v_min); // B at start to indicate battery level, limit id to 4 chars, and voltages to 2 decimal places
    wearable.write(str);
  
    DateTime now = rtc.now(); // control to make it so the list isn't sent unless it has been more than 5 minutes
    last_sent = now.unixtime();
  
    memset(str, 0, sizeof(str)); // clear the str buffer
  
    // Once the list is sent, clear all lists (records and kalman)
    memset(records, 0, sizeof(records));
    memset(kalmans, 0, sizeof(kalmans));
  
    for (int i=0; i<ARRAY_SIZE; i++)
    {
      records[i].rssi = -128;
      records[i].filtered_rssi = -128;
    }
    Bluefruit.disconnect(conn_handle);
  }
  else
  {
//    char dont_connect[2] = "x"; // sends a don't connect flag
//    wearable.write(dont_connect);
    Bluefruit.disconnect(conn_handle);
  }

  
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
  // SET RTC STUFF HERE
  // Time
  DateTime now = rtc.now();
  record.first = now.unixtime(); // use unix time to calculate duration of contact - this is overwritten in the insertRecord() function if a record already exists
  record.last = now.unixtime();
  record.hour = now.hour();
  record.minute = now.minute();
  
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
    Serial.printf("%.4s ",records[i].name);
    Serial.printf("%i (%u)\n", records[i].filtered_rssi, records[i].duration);
  }
}


/* This function attempts to insert the record if there is room */
/* Returns 1 if a change was made, otherwise 0 */
int insertRecord(node_record_t *record) // need to add some code for checking the rssi threshold in this function (after filtering)
{
  DateTime now = rtc.now();
  uint8_t i;
  
  /*    Check for a match on existing device address */
  /*    Filter the RSSI */
  /*    Check if RSSI>Threshold */
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
      //if(((record->last) - (records[i].last)) < (TIMEOUT)) // if the ble device has been detected within the last 60 seconds, keep it in the same record - otherwise start a new interaction
      if(1)
      {
        // Update raw RSSI then filter
        updateRaw(&kalmans[i], record->rssi);
        filter(&kalmans[i]);
        record->filtered_rssi = kalmans[i].filtered;
  
        // if there is a previous record for this ID AND the previous record doesn't have data in the 'last' field
        if ((records[i].first < record->first) && (records[i].first!=0))
        {
         record->first = records[i].first;
         record->hour = records[i].hour;
         record->minute = records[i].minute;
        }
        record->last = now.unixtime();
        updateDuration(record);

        if(record->filtered_rssi >= RSSI_THRESHOLD ||)
        {
          memcpy(&records[i], record, sizeof(node_record_t));
          goto inserted;
        }
      }
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
      if(record->filtered_rssi >= RSSI_THRESHOLD)
      {
        memcpy(&records[i], record, sizeof(node_record_t));
        goto inserted;
      }
    }
  }


  // Nothing to do ... list is either full or RSSI not high enough
  return 0;

inserted:
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

void setUpKalman(kal *k) // parameters are updated pre-meeting with Mehmet on 30/09/20
{
  k->meas_uncertainty = 2.2398; // the variance of static signal
  k->est_uncertainty = k->meas_uncertainty;
  k->q = 0.05;
}

void updateRaw(kal *k, uint8_t rssi)
{
  k->raw = rssi;
}

void updateDuration(node_record_t *record)
{
  if(record->last > record->first)
  {
    record->duration = abs((record->last) - (record->first));
  }
//  else
//  {
//    record->duration = 0;
//  }
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
  }
