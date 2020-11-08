clear all; close all; clc;
k=1;
% These are for 100% scans, tx_power=-8dBm
fileID = fopen('adv_interval\always_scan_100msadv.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure1 = figure('Name','Comparison of Advertising Intervals');
subplot(3,2,k);
sgtitle('Advertising Interval Analysis');
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
title('Advertising Interval = 100ms');
k = k+1;

fileID = fopen('adv_interval\always_scan_250msadv.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
subplot(3,2,k);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
title('Advertising Interval = 250ms');
k = k+1;

fileID = fopen('adv_interval\always_scan_500msadv.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
subplot(3,2,k);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
title('Advertising Interval = 500ms');
k = k+1;

fileID = fopen('adv_interval\always_scan_750msadv.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
subplot(3,2,k);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
title('Advertising Interval = 750ms');
k = k+1;

fileID = fopen('adv_interval\always_scan_1000msadv.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
subplot(3,2,k);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
title('Advertising Interval = 1000ms');

for i=1:5
    subplot(3,2,i);
    xlabel('Time (s)');
    ylabel('RSSI');
    xlim([0 73]);
    x1=xline(0,'-',{'0.2m'});
    x2=xline(11,'-',{'0.4m'});
    x3=xline(21,'-',{'0.6m'});
    x4=xline(31,'-',{'0.8m'});
    x5=xline(41,'-',{'1.0m'});
    x6=xline(51,'-',{'1.5m'});
    x7=xline(61,'-',{'2.0m'});
    x1.LabelVerticalAlignment = 'bottom';
    x2.LabelVerticalAlignment = 'bottom';
    x3.LabelVerticalAlignment = 'bottom';
    x4.LabelVerticalAlignment = 'bottom';
    x5.LabelVerticalAlignment = 'bottom';
    x6.LabelVerticalAlignment = 'bottom';
    x7.LabelVerticalAlignment = 'bottom';
    x1.LabelOrientation = 'horizontal';
    x2.LabelOrientation = 'horizontal';
    x3.LabelOrientation = 'horizontal';
    x4.LabelOrientation = 'horizontal';
    x5.LabelOrientation = 'horizontal';
    x6.LabelOrientation = 'horizontal';
    x7.LabelOrientation = 'horizontal';
end