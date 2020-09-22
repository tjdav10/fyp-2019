clear all; close all; clc;

% reading from file
fileID = fopen('phone.log');
data = fscanf(fileID,'%d');

% Computing variance of RSSI (measurement uncertainity)
var_rssi = var(data);

% Declaring parameters - adjust these to tune Kalman Filter
meas_uncertainty = var_rssi; % measurement uncertainty (covariance of signal)
est_uncertainty = meas_uncertainty; % estimate uncertainty (set same as meas_uncertainty initially. is updated in the filter)
R = 0.00000001; % process noise
% reduce the process noise
% try varying the advertising intervals to make it faster

window_size = 30; % set window size for moving average filter
% increase the advertising interval

% temporary
% meas = 0;
% prev_est = 0;

% Arrays set up
% data = zeros(1,1000); % replace to read from file
kalman_out = zeros(1,length(data));

% in loop
for i=1:length(data)
    meas = data(i); % load current RSSI into meas
    if i==1
        prev_est = data(i); % on first iteration
    end
    % Kalman steps
    kal_gain = est_uncertainty/(est_uncertainty + meas_uncertainty); % compute kalman gain
    cur_est = prev_est + kal_gain*(meas - prev_est); % compute estimate for current time step
    est_uncertainty = (1 - kal_gain)*est_uncertainty + abs(prev_est - cur_est)*R; % update estimate uncertainty
    prev_est = cur_est; % update previous estimate for next loop iteration
    kalman_out(i) = (cur_est);
end

% Following function computes moving average using window_size as set
% previously. Where there are fewer than window_size values, it uses the
% values available (e.g. 1st entry is window size 1, 2nd entry window
% size 2 etc.)

m_avg_out = (movmean(data,[(window_size)-1 0]));

plot(data);
title('Comparison of Filters on BLE RSSI');
ylabel('RSSI');
xlabel('Time');
hold on;
plot(kalman_out);
plot(m_avg_out);
legend('raw','kalman','moving avg');
% legend('kalman','moving avg');

var_mov = var(m_avg_out);
var_kal = var(kalman_out);
fprintf("Variance of moving average (window size=%i) = %f\n",window_size,var_mov);
fprintf("Variance of kalman filter = %f",var_kal);