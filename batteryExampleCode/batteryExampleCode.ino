int adcin    = A6;  // A6 == VDIV is for battery voltage monitoring 
int adcvalue = 0;
float mv_per_lsb = 3600.0F/4096.0F; // 12-bit ADC with 3.6V input range
                                    //the default analog reference voltage is 3.6 V

void setup() {
  analogReadResolution(12); // Can be 8, 10, 12 or 14
  delay(5);
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
}

void loop() {
  adcvalue = analogRead(adcin);
  Serial.print("ADC: ");
  Serial.print(adcvalue);
  Serial.print(";  Battery voltage: ");
  Serial.println((float)adcvalue * mv_per_lsb/1000*2);
  delay(3000);
}
