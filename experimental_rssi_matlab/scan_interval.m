clear all; close all; clc;
% tx power is -8dBm same as scan power
% adv_interval is 500ms
fileID = fopen('scan_interval\s100ms_int_50ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(1);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=50ms');

fileID = fopen('scan_interval\s100ms_int_60ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(2);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=60ms');

fileID = fopen('scan_interval\s100ms_int_70ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(3);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=70ms');


fileID = fopen('scan_interval\s100ms_int_80ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(4);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=80ms');


fileID = fopen('scan_interval\s100ms_int_90ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(5);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=90ms');

fileID = fopen('scan_interval\s100ms_int_100ms_window.log');
data = fscanf(fileID,'%d %d', [2 Inf]);
data = data';
offset = data(1,1);
data(:,1) = data(:,1) - offset;
data(:,1) = data(:,1)/1000;
figure(6);
plot(data(:,1),data(:,2));
fclose(fileID);
hold on;
xlabel('Time (s)');
ylabel('RSSI');
title('Scan interval=100ms, window=100ms');

