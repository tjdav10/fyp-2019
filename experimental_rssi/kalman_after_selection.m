clear all; close all; clc;
% tx power is -8dBm same as scan power
% adv_interval is 500ms, scan window is 100% of time

window_size = 10;

fileID = fopen('kalman\log_9_october.log');
nine_october=1;
moving_longer=0;

% fileID = fopen('kalman\moving_longer.log');
% moving_longer=1;
% nine_october=0;

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

fileID_static = fopen('kalman\kalman_150cm_static.log');
data_static = fscanf(fileID_static,'%d %d', [2 Inf]);
data_static = data_static';
var_rssi=var(data_static(:,2));
fclose(fileID_static);

% Declaring parameters - adjust these to tune Kalman Filter
meas_uncertainty = var_rssi; % measurement uncertainty (covariance of signal)
est_uncertainty = meas_uncertainty; % estimate uncertainty (set same as meas_uncertainty initially. is updated in the filter)
R = [1 0.5 0.25 0.1 0.075 0.05]; % process noise

% fileID = fopen('kalman\moving_longer.log');
% data = fscanf(fileID,'%d %d', [2 Inf]);
% data = data';
% offset = data(1,1);
% data(:,1) = data(:,1) - offset;
% data(:,1) = data(:,1)/1000;
% fclose(fileID);
% 
% m_avg_out = round((movmean(data(:,2),[(window_size)-1 0])));
time(:,1) = data(:,1);

figure('Name', 'Process Noise Analysis');
subplot(ceil(length(R)/2),2,1);
sgtitle('Process Noise Analysis');


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
    if(moving_longer==1)
        x1=xline(0,'-',{'0.5m'});
        x2=xline(30,'-',{'1.0m'});
        x3=xline(63,'-',{'1.5m'});
        x4=xline(93,'-',{'2.0m'});
        xlim([0 122])
    elseif(nine_october==1)
        x1=xline(0,'-',{'0.5m'});
        x2=xline(61,'-',{'1.0m'});
        x3=xline(124,'-',{'1.5m'});
        x4=xline(184,'-',{'2.0m'});
        xlim([0 243])
    end
    x1.LabelVerticalAlignment = 'bottom';
    x2.LabelVerticalAlignment = 'bottom';
    x3.LabelVerticalAlignment = 'bottom';
    x4.LabelVerticalAlignment = 'bottom';
    
    x1.LabelOrientation = 'horizontal';
    x2.LabelOrientation = 'horizontal';
    x3.LabelOrientation = 'horizontal';
    x4.LabelOrientation = 'horizontal';
end

