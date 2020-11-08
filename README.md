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
## BLE Router
`todo`

### Router Flowchart
![BLE Router Flowchart](/router.png)
## Gateway
`todo`

### Gateway Flowchart
![Gateway Flowchart](/gateway.png)
