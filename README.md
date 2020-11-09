# Activity Tracking for Hand Hygiene IoT Project
This repo has the source code for a system that tracks and records the names of wearable BLE devices that have been within 1.5m. There are three main components:
- `wearable_ble.ino` (for wearable BLE ID card)
- `router_ble.ino` (for the BLE router)
- `gateway_cloud.py` (for the gateway)

## Hardware
The wearable BLE ID card and the BLE router both use the [Adafruit PCF8523 Real Time Clock Assembled Breakout Board](https://www.adafruit.com/product/3295). They will not work if the RTC is not connected when the devices are powered on.

The file `wearable_ble.ino` will run on the BLE boards in the wearable BLE ID card from the Hand Hygiene IoT project.

The file `router_ble.ino` will run on the same type of BLE board. It can also run on the Adafruit Feather nRF52 Bluefruit board.

The file `gateway_cloud.py` runs using Python 3.7. I recommend running it on a Raspberry Pi.

## Wearable ID Card
Follow [this guide](https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather/overview) to set up the BLE boards and the Arduino environment (for wearable ID card and router).

### Contact Tracing Flowchart
![Wearable Contact Tracing Flowchart](/wearable_contact_tracing.png)

### Wearable to Router Flowchart
![Wearable to Router Connection Flowchart](/wearable_connection.png)

### Structs
#### node_record_t
```C
{
typedef struct node_record_s
{
  // BLE Tag attributes
  uint8_t  addr[6];    // Six byte device address
  char name[4];       // Name of detected device
  // RTC attributes (timestamps all unixtime)
  uint32_t first; // first timestamp
  uint32_t last; // last seen timestamp
  uint32_t first_close; // timestamp they entered 1.5m proximity
  uint32_t last_close; // timestamp they were last seen in 1.5m proximity
  uint32_t duration; //duration of contact in seconds
  uint32_t duration_close; //duration of within rssi_proximity
  uint32_t temp_duration;
  uint8_t hour;
  uint8_t minute;
  // Kalman Parameters
  float meas_uncertainty;
  float est_uncertainty;
  float q;
  // Kalman Variables
  float prev_est;
  float cur_est;
  float kal_gain;
  int8_t rssi; // current RSSI
  int8_t filtered_rssi;       // min RSSI value
  //int8_t   reserved;   // Padding for word alignment
  bool complete; // records if this log is complete - if true, do not modify this entry. start a new one
  bool converged; // enough time has passed for Kalman filter to have converged
  bool within_threshold; // within proximity
  bool time_stored;
} node_record_t;
}
  ```
  
There is a global array of `node_record_t`s called `records`.

### Functions
#### startAdv()
* Adds UUID, BLEUART Service and Name to the advertising packet.
* Advertises every 500ms

#### connect_callback(uint16_t conn_handle)
* Calls `Bluefruit.Advertising.start()` and `Bluefruit.Scanner.start(0)` so that scanning and advertising isn't paused while connected
* Responsible for checking to see if list was sent in last 5 minutes
* Formats information from the node_record_t struct into a string for transmission
* Also computes the battery level data, formats into string, and sends
* Once list has been sent, the list is reset and Kalman filter is reinitialised
* Then it disconnects itself using `Bluefruit.disconnect(conn_handle);`

### disconnect_callback(uint16_t conn_handle, uint8_t reason)
* Called when the device disconnects

### scan_callback(ble_gap_evt_adv_report_t* report)
* Called when a valid advertising packet is detected (correct UUID)
* Copies information from packet such as name, RSSI into a local `node_record_t`
* Calls `insertRecord` on a pointer to the local record.
```C
{
  /* Attempt to insert the record into the list */
  if (insertRecord(&record) == 1)                 /* Returns 1 if the list was updated */
  {
    printRecordList();                            /* The list was updated, print the new values */
  }
}
```
* Resumes scanning at end with `Bluefruit.Scanner.resume()`;

### printRecordList()
* Used for debugging
* Prints current record list to the serial monitor

### int insertRecord(node_record_t *record)
1. Check for a match in the global array `records`
2. Check to see if record has timed out
  * If timed out, create a new record
  * If not timed out, update record
3. Filter the RSSI
4. Check if convergence time has elapsed
  * If it has, check if RSSI is above threshold
5. Update times and durations

* Returns 1 if list is updated
* Returns 0 if list not updated

### filter(node_record_t *k)
*Runs one iteration of the Kalman filter

### setUpKalman(node_record_t *k)
* initialises the measurement uncertainty, estimate uncertainty and process noise

### updateRaw(node_record_t *k, uint8_t rssi)
* Updates the raw RSSI of a `node_record_t struct`

### updateDuration(node_record_t *record)
* Updates the duration in a `node_record_t` struct

## BLE Router
* BLE Router requires a RTC as mentioned above.
### Structs
#### prph_info_t
```C
{
// Each peripheral needs its own bleuart client service
typedef struct
{
  char name[16+1];

  uint16_t conn_handle;

  // Each prph need its own bleuart client service
  BLEClientUart bleuart;
} prph_info_t;
}
```
#### sent_timer
```C
{
typedef struct
{
  char name[4+1];
  uint32_t last_time;
} sent_timer;

const uint8_t CUSTOM_UUID[] =
{
    0xA0, 0xDB, 0xD3, 0x6A, 0x00, 0xA6, 0xF7, 0x8C,
    0xE7, 0x11, 0x8F, 0x71, 0x1A, 0xFF, 0x67, 0xDF
};
}
```
### Functions
#### scan_callback(ble_gap_evt_adv_report_t* report)
* Called when a valid advertising packet from a wearable is detected
* Checks to see if device has been connected to within last 5 minutes

#### connect_callback(uint16_t conn_handle)
* Called when connecting to device
* Selects corresponding BLE UART service for the device connected to

### Router Flowchart
![BLE Router Flowchart](/router.png)
### Notes
* There is probably some ununnecessary code in this file, as I was rushing to try and fix the router before the end of my project. But it works as intended.
* Specifically, it may not be necessary to have the global arrays because there is only one connection at a time. But this may need to be tested
* Is **not** integrated with LoRa at the moment but should be compatible
## Gateway
* Reads strings from a serial port and uploads to cloud MySQL server
* Two types of strings from this project, battery strings and interaction strings
* Example battery string: `B D001 98.99` means that D001 has battery level of 98.99%
* Battery value is rounded to whole number
* The `B` at the start of string is there as that is what the Python script checks
* Example interaction string: `D001 D002 478 16:14` means that this string is from D001, they are reporting they were within 1.5m of D002 for 478 seconds starting at 16:14.
* Date is taken from RasPi and sent to database.

### Gateway Flowchart
![Gateway Flowchart](/gateway.png)
