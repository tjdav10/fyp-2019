clear all; close all; clc;
% tx power is -8dBm same as scan power
% adv_interval is 500ms, scan window is 100% of time

window_size = 5;
fileID = fopen('kalman\kalman_150cm.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
fclose(fileID);
% figure(1);
% plot(data(:,1),data(:,2));
% hold on;
% xlabel('Time (s)');
% ylabel('RSSI');
% title('150cm static');

var_rssi=var(data(:,2));

% Declaring parameters - adjust these to tune Kalman Filter
meas_uncertainty = var_rssi; % measurement uncertainty (covariance of signal)
est_uncertainty = meas_uncertainty; % estimate uncertainty (set same as meas_uncertainty initially. is updated in the filter)
R = [1 0.5 0.25 0.1 0.05 0.025]; % process noise

fileID = fopen('kalman\moving_longer.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
fclose(fileID);

m_avg_out = round((movmean(data(:,2),[(window_size)-1 0])));
time(:,1) = data(:,1);

figure(10)
subplot(ceil(length(R)/2),2,1);


for j=1:length(R)
% in loop
    for i=1:length(data(:,2))
        meas = data(i,2); % load current RSSI into meas
        if i==1
            prev_est = data(i,2); % on first iteration
        end
        % Kalman steps
        kal_gain = est_uncertainty/(est_uncertainty + meas_uncertainty); % compute kalman gain
        cur_est = prev_est + kal_gain*(meas - prev_est); % compute estimate for current time step
        est_uncertainty = (1 - kal_gain)*est_uncertainty + abs(prev_est - cur_est)*R(j); % update estimate uncertainty
        prev_est = cur_est; % update previous estimate for next loop iteration
        kalman_out(i,j) = (cur_est);
    end
    kalman_out(:,j) = round(kalman_out(:,j));
    subplot(ceil(length(R)/2),2,j)
    plot(data(:,1),data(:,2));
    hold on;
    plot(time,kalman_out(:,j));
%     plot(time,m_avg_out);
    xlabel('Time (s)');
    ylabel('RSSI');
    title(['Process noise = ',num2str(R(j))]);
end

