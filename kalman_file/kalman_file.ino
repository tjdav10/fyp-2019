/* Kalman function file
 * TODO
 * Check and see if I need to use floats or if I can mix floats and int8_t (it compiles but need to test it)
 * 
 * 
 */
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
  int8_t out;
  int8_t meas;
} kal;

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}

// Kalman function
float filter(kal *k) // pass by reference to save memory
{
  k->kal_gain = k->est_uncertainty/(k->est_uncertainty + k->meas_uncertainty); // compute kalman gain
  k->cur_est = k->prev_est + k->kal_gain*(k->meas - k->prev_est); // compute estimate for current time step
  k->est_uncertainty = (1 - k->kal_gain)*k->est_uncertainty + abs(k->prev_est - k->cur_est)*k->q; // update estimate uncertainty
  k->prev_est = k->cur_est; // update previous estimate for next loop iteration
  return k->out;
}
