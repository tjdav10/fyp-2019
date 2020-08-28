clear all; close all; clc;

% reading from file
fileID = fopen('data.log');
data = fscanf(fileID,'%d');

% Declaring parameters - adjust these to tune Kalman Filter
meas_uncertainty = 2; % measurement uncertainty
est_uncertainty = 2; % estimate uncertainty
q = 0.01; % process noise

window_size = 20; % set window size for moving average filter

% temporary
% meas = 0;
% prev_est = 0;

% Arrays set up
% data = zeros(1,1000); % replace to read from file
kalman_out = zeros(1,1000);

% in loop
for i=1:length(data)
    meas = data(i); % load current RSSI into meas
    if i==1
        prev_est = 0; % on first iteration
    end
    % Kalman steps
    kal_gain = est_uncertainty/(est_uncertainty + meas_uncertainty); % compute kalman gain
    cur_est = prev_est + kal_gain*(meas - prev_est); % compute estimate for current time step
    est_uncertainty = (1 - kal_gain)*est_uncertainty + abs(prev_est - cur_est)*q; % update estimate uncertainty
    prev_est = cur_est; % update previous estimate for next loop iteration
    kalman_out(i) = cur_est;
end

% Following function computes moving average using window_size as set
% previously. Where there are fewer than window_size values, it uses the
% values available (e.g. 1st entry is window size 1, 2nd entry window
% size 2 etc.)

m_avg_out = movmean(data,[(window_size)-1 0]);

plot(data);
hold on;
plot(kalman_out);
plot(m_avg_out);