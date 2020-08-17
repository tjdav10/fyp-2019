/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution

 Author: KTOWN (Kevin Townsend)
 Copyright (C) Adafruit Industries 2017
*********************************************************************/

/*  This example scans for advertising devices (peripherals) in range,
 *  looking for a specific UUID in the advertising packet. When this
 *  UUID is found, it will display an alert, sorting devices detected
 *  in range by their RSSI value, which is an approximate indicator of
 *  proximity (though highly dependent on environmental obstacles, etc.).
 *  
 *  A simple 'bubble sort' algorithm is used, along with a simple
 *  set of decisions about whether and where to insert new records
 *  in the record list.
 *  
 *  This example is intended to be used with multiple peripherals
 *  devices running the *_peripheral.ino version of this application.
 *  
 *  VERBOSE_OUTPUT
 *  --------------
 *  Verbose advertising packet analysis can be enabled for
 *  advertising packet contents to help debug issues with the 
 *  advertising payloads, with fully parsed data displayed in the 
 *  Serial Monitor.
 *  
 *  ARRAY_SIZE
 *  ----------
 *  The numbers of peripherals tracked and sorted can be set via the
 *  ARRAY_SIZE macro. Must be at least 2.
 *  
 *  TIMEOUT_MS
 *  ----------
 *  This value determines the number of milliseconds before a tracked
 *  peripheral has it's last sample 'invalidated', such as when a device
 *  is tracked and then goes out of range.
 *  
 *  ENABLE_TFT
 *  ----------
 *  An ILI9341 based TFT can optionally be used to display proximity 
 *  alerts. The Adafruit TFT Feather Wing is recommended and is the only 
 *  device tested with this functionality.
 *  
 *  ENABLE_OLED
 *  -----------
 *  An SSD1306 128x32 based OLED can optionally be used to display
 *  proximity alerts. The Adafruit OLED Feather Wing is recommended and
 *  is the only device tested with this functionality.
 */

#include <string.h>
#include <bluefruit.h>
#include <ble_gap.h>
#include <SPI.h>

#define VERBOSE_OUTPUT (0)    // Set this to 1 for verbose adv packet output to the serial monitor
#define ARRAY_SIZE     (4)    // The number of RSSI values to store and compare
#define TIMEOUT_MS     (2500) // Number of milliseconds before a record is invalidated in the list
#define ENABLE_TFT     (0)    // Set this to 1 to enable ILI9341 TFT display support
#define ENABLE_OLED    (0)    // Set this to 1 to enable SSD1306 128x32 OLED display support


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
  int8_t   rssi;       // RSSI value
  uint32_t timestamp;  // Timestamp for invalidation purposes
  char name[4];       // Name of detected device
  uint8_t count;
  int8_t   reserved;   // Padding for word alignment
} node_record_t;

bool connected; // use to see if wearable is connected to central

node_record_t records[ARRAY_SIZE];

// Add BLE services
BLEUart wearable;       // uart over ble, as the peripheral

void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("Contact tracing proximity app");
  Serial.println("-------------------------------------\n");

  /* Clear the results list */
  memset(records, 0, sizeof(records));
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    // Set all RSSI values to lowest value for comparison purposes,
    // since 0 would be higher than any valid RSSI value
    records[i].rssi = -128;
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
  
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values

  /* Set the device name */
  Bluefruit.setName("D001");

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
  Bluefruit.Scanner.filterRssi(-80);            // Only invoke callback for devices with RSSI >= -80 dBm
  Bluefruit.Scanner.filterUuid(uuid);           // Only invoke callback if the target UUID was found
  //Bluefruit.Scanner.filterMSD(0xFFFF);          // Only invoke callback when MSD is present with the specified Company ID
  Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
  Bluefruit.Scanner.useActiveScan(true);        // Request scan response data
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
  Bluefruit.Advertising.setInterval(32, 244);    // in units of 0.625 ms
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
  connected = true;
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.println("Disconnected");
  connected = false;
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
  //Serial.println(record.name);
  record.timestamp = millis();                    /* Set the timestamp (approximate) */

  /* Attempt to insert the record into the list */
  if (insertRecord(&record) == 1)                 /* Returns 1 if the list was updated */
  {
    printRecordList();                            /* The list was updated, print the new values */
    Serial.println("");
  }

/* Fully parse and display the advertising packet to the Serial Monitor
 * if verbose/debug output is requested */
#if VERBOSE_OUTPUT
  uint8_t len = 0;
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));

  /* Display the timestamp and device address */
  if (report->type.scan_response)
  {
    Serial.printf("[SR%10d] Packet received from ", millis());
  }
  else
  {
    Serial.printf("[ADV%9d] Packet received from ", millis());
  }
  // MAC is in little endian --> print reverse
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.println("");
  
  /* Raw buffer contents */
  Serial.printf("%14s %d bytes\n", "PAYLOAD", report->data.len);
  if (report->data.len)
  {
    Serial.printf("%15s", " ");
    Serial.printBuffer(report->data.p_data, report->data.len, '-');
    Serial.println();
  }

  /* RSSI value */
  Serial.printf("%14s %d dBm\n", "RSSI", report->rssi);

  /* Adv Type */
  Serial.printf("%14s ", "ADV TYPE");
  if ( report->type.connectable )
  {
    Serial.print("Connectable ");
  }else
  {
    Serial.print("Non-connectable ");
  }

  if ( report->type.directed )
  {
    Serial.println("directed");
  }else
  {
    Serial.println("undirected");
  }

  /* Shortened Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "SHORT NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Complete Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "COMPLETE NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* TX Power Level */
  if (Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %i\n", "TX PWR LEVEL", buffer[0]);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Check for UUID16 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID16 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID128 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }

  /* Check for UUID128 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }  

  /* Check for BLE UART UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, BLEUART_UUID_SERVICE) )
  {
    Serial.printf("%14s %s\n", "BLE UART", "UUID Found!");
  }

  /* Check for DIS UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, UUID16_SVC_DEVICE_INFORMATION) )
  {
    Serial.printf("%14s %s\n", "DIS", "UUID Found!");
  }

  /* Check for Manufacturer Specific Data */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, buffer, sizeof(buffer));
  if (len)
  {
    Serial.printf("%14s ", "MAN SPEC DATA");
    Serial.printBuffer(buffer, len, '-');
    Serial.println();
    memset(buffer, 0, sizeof(buffer));
  }

  Serial.println();
#endif

  // For Softdevice v6: after received a report, scanner will be paused
  // We need to call Scanner resume() to continue scanning
  Bluefruit.Scanner.resume();
}

/* Prints a UUID16 list to the Serial Monitor */
void printUuid16List(uint8_t* buffer, uint8_t len)
{
  Serial.printf("%14s %s", "16-Bit UUID");
  for(int i=0; i<len; i+=2)
  {
    uint16_t uuid16;
    memcpy(&uuid16, buffer+i, 2);
    Serial.printf("%04X ", uuid16);
  }
  Serial.println();
}

/* Prints a UUID128 list to the Serial Monitor */
void printUuid128List(uint8_t* buffer, uint8_t len)
{
  (void) len;
  Serial.printf("%14s %s", "128-Bit UUID");

  // Print reversed order
  for(int i=0; i<16; i++)
  {
    const char* fm = (i==4 || i==6 || i==8 || i==10) ? "-%02X" : "%02X";
    Serial.printf(fm, buffer[15-i]);
  }

  Serial.println();  
}

/* Prints the current record list to the Serial Monitor */
void printRecordList(void)
{
  for (uint8_t i = 0; i<ARRAY_SIZE; i++)
  {
    Serial.printf("[%i] ", i);
    //Serial.printBuffer(records[i].addr, 6, ':');
    Serial.printf("%.4s ",records[i].name);
    Serial.printf("%i ",records[i].count);
    Serial.printf("%i (%u ms)\n", records[i].rssi, records[i].timestamp);
  }
}

/* This function performs a simple bubble sort on the records array */
/* It's slow, but relatively easy to understand */
/* Sorts based on RSSI values, where the strongest signal appears highest in the list */
void bubbleSort(void)
{
  int inner, outer;
  node_record_t temp;

  for(outer=0; outer<ARRAY_SIZE-1; outer++)
  {
    for(inner=outer+1; inner<ARRAY_SIZE; inner++)
    {
      if(records[outer].rssi < records[inner].rssi)
      {
        memcpy((void *)&temp, (void *)&records[outer], sizeof(node_record_t));           // temp=records[outer];
        memcpy((void *)&records[outer], (void *)&records[inner], sizeof(node_record_t)); // records[outer] = records[inner];
        memcpy((void *)&records[inner], (void *)&temp, sizeof(node_record_t));           // records[inner] = temp;
      }
    }
  }
}

/*  This function will check if any records in the list
 *  have expired and need to be invalidated, such as when
 *  a device goes out of range.
 *  
 *  Returns the number of invalidated records, or 0 if
 *  nothing was changed.
 */
int invalidateRecords(void)
{
  uint8_t i;
  uint8_t buffer[4+1]; // used for names of 4 chars long
  int match = 0;

  /* Not enough time has elapsed to avoid an underflow error */
  if (millis() <= TIMEOUT_MS)
  {
    return 0;
  }

  /* Check if any records have expired */
  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (records[i].timestamp) // Ignore zero"ed records
    {
      if (millis() - records[i].timestamp >= TIMEOUT_MS)
      {
        /* Record has expired, zero it out */
        memset(&records[i], 0, sizeof(node_record_t));
        records[i].rssi = -128;
        match++;
      }
    }
  }

  /* Resort the list if something was zero'ed out */
  if (match)
  {
    // Serial.printf("Invalidated %i records!\n", match);
    bubbleSort();    
  }

  return match;
}

/* This function attempts to insert the record if it is larger than the smallest valid RSSI entry */
/* Returns 1 if a change was made, otherwise 0 */
int insertRecord(node_record_t *record)
{
  uint8_t i;
  
  /* Invalidate results older than n milliseconds */
  invalidateRecords();
  
  /*  Record Insertion Workflow:
   *  
   *            START
   *              |
   *             \ /
   *        +-------------+
   *  1.    | BUBBLE SORT |   // Put list in known state!
   *        +-------------+
   *              |
   *        _____\ /_____
   *       /    ENTRY    \    YES
   *  2. <  EXISTS W/THIS > ------------------+
   *       \   ADDRESS?  /                    |
   *         -----------                      |
   *              | NO                        |
   *              |                           |
   *       ______\ /______                    |
   *      /      IS       \   YES             |
   *  3. < THERE A ZERO'ED >------------------+
   *      \    RECORD?    /                   |
   *        -------------                     |
   *              | NO                        |
   *              |                           |
   *       ______\ /________                  |
   *     /     IS THE       \ YES             |
   *  4.<  RECORD'S RSSI >=  >----------------|
   *     \ THE LOWEST RSSI? /                 |
   *       ----------------                   |
   *              | NO                        |
   *              |                           |
   *             \ /                         \ /
   *      +---------------+           +----------------+
   *      | IGNORE RECORD |           | REPLACE RECORD |
   *      +---------------+           +----------------+
   *              |                           |
   *              |                          \ /
   *             \ /                  +----------------+
   *             EXIT  <------------- |   BUBBLE SORT  |
   *                                  +----------------+
   */  

  /* 1. Bubble Sort 
   *    This puts the lists in a known state where we can make
   *    certain assumptions about the last record in the array. */
  bubbleSort();

  /* 2. Check for a match on existing device address */
  /*    Replace it if a match is found, then sort */
  uint8_t match = 0;
  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (memcmp(records[i].addr, record->addr, 6) == 0)
    {
      match = 1;
    }
    if (match)
    {
      memcpy(&records[i], record, sizeof(node_record_t));
      records[i].count += 1;
      goto sort_then_exit;
    }
  }

  /* 3. Check for zero'ed records */
  /*    Insert if a zero record is found, then sort */
  for (i=0; i<ARRAY_SIZE; i++)
  {
    if (records[i].rssi == -128)
    {
      memcpy(&records[i], record, sizeof(node_record_t));
      goto sort_then_exit;
    }
  }

  /* 4. Check RSSI of the lowest record */
  /*    Replace if >=, then sort */
  if (records[ARRAY_SIZE-1].rssi <= record->rssi)
  {
      memcpy(&records[ARRAY_SIZE-1], record, sizeof(node_record_t));
      goto sort_then_exit;
  }

  /* Nothing to do ... RSSI is lower than the last value, exit and ignore */
  return 0;

sort_then_exit:
  /* Bubble sort */
  bubbleSort();
  return 1;
}

void loop() 
{


  // following block sends names on list over Bluetooth ignoring blank entries
  // Trying to send list over Bluetooth - it works!!
  uint8_t buf[4]; // uint8_t buffer to use in wearable.write
  if(connected)
  {
    for (int i=0; i<ARRAY_SIZE; i++)
    {
      if (records[i].rssi!=-128) // checks to see if there is actually a device in this spot, if not nothing happens
      {
        memcpy(buf, records[i].name, sizeof(buf)); // copy contents of name to buffer
        wearable.write(buf, sizeof(buf)); // write name of device
        memset(buf, 0, sizeof(buf)); // reset buffer
        //memcpy(buf, records[i].count, sizeof(buf)); // copy count integer to buffer
        wearable.write(records[i].count); // write count of device from list - this does send it but count doesn't work as I thought it would
        //memset(buf, 0, sizeof(buf)); // reset buffer
      }
    }
  }
  
  // Forward from Serial to BLEUART
  if (Serial.available())
  {
    // Delay to get enough input data since we have a
    // limited amount of space in the transmit buffer
    delay(2);
 
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
  if (invalidateRecords())
  {
    /* The list was updated, print the new values */
    //printRecordList();
    //sendRecordList();
    Serial.println("");
    /* Display the device list on the TFT if available */
    #if ENABLE_TFT
    renderResultsToTFT();
    #endif
    /* Display the device list on the OLED if available */
    #if ENABLE_OLED
    renderResultsToOLED();
    #endif
  }
}
