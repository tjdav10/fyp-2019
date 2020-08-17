void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}

int kalman_rssi(int measured)
{
  // Parameters
  int F = 1;
  int B = 0;
  int u = 0;
  float Q = 0.008; // Q is process noise 0.008 (very low, from Wouter Bulten article)
  float R = 0.3; // R is observation noise (use the variance of BLE from actual data to determine this value)
  int H = 1; % output voltage is only observable
  int p = 1;
  int Xp = 0;
  int Zp = 0;
  int Xe = 0;
  float Pc = p + Q; // p is covariance
  K = Pc/(Pc + R); //computing kalman gain
  p = (1-K)*Pc; // updating covariance
  Xp = Xe;
  Zp = Xp;
  Xe = K*(measured-Zp) + Xp; //updating kalman estimate using prediction and measurement
  
  return Xe;
}
