% Advertising interval is 20ms, scan all the time
clear all; close all; clc;

figure1=figure('Name', 'Comparison of TX Power');
% subplot(5,2,1);
% fileID = fopen('better_8.log');
% data = fscanf(fileID,'%d %d', [2 Inf]);
% data = data';
% offset = data(1,1);
% data(:,1) = data(:,1) - offset;
% data(:,1) = data(:,1)/1000;
% plot(data(:,1),data(:,2));
% fclose(fileID);
% title('TxPower=8dBm');

k=1;
h=4;

subplot(h,2,k);
sgtitle('Comparison of TX Power');
fileID = fopen('better_4.log');
data1 = fscanf(fileID,'%d %d', [2 Inf]);
data1= data1';
offset = data1(1,1);
data1(:,1) = data1(:,1) - offset;
data1(:,1) = data1(:,1)/1000;
plot(data1(:,1),data1(:,2));
fclose(fileID);
title('TxPower=4dBm');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_2.log');
data2 = fscanf(fileID,'%d %d', [2 Inf]);
data2= data2';
offset = data2(1,1);
data2(:,1) = data2(:,1) - offset;
data2(:,1) = data2(:,1)/1000;
plot(data2(:,1),data2(:,2));
fclose(fileID);
title('TxPower=2dBm');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_0.log');
data3 = fscanf(fileID,'%d %d', [2 Inf]);
data3= data3';
offset = data3(1,1);
data3(:,1) = data3(:,1) - offset;
data3(:,1) = data3(:,1)/1000;
plot(data3(:,1),data3(:,2));
fclose(fileID);
title('TxPower=0dBm');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_-4.log');
data4 = fscanf(fileID,'%d %d', [2 Inf]);
data4= data4';
offset = data4(1,1);
data4(:,1) = data4(:,1) - offset;
data4(:,1) = data4(:,1)/1000;
plot(data4(:,1),data4(:,2));
fclose(fileID);
title('TxPower=-4dBm');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_-8.log');
data5 = fscanf(fileID,'%d %d', [2 Inf]);
data5= data5';
offset = data5(1,1);
data5(:,1) = data5(:,1) - offset;
data5(:,1) = data5(:,1)/1000;
plot(data5(:,1),data5(:,2));
fclose(fileID);
title('TxPower=-8dBm');
annotation(figure1,'ellipse',...
    [0.354061224489796 0.106382978723404 0.125530612244898 0.105263157894737],...
    'Color',[1 0 0],...
    'LineStyle','--');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_-12.log');
data6 = fscanf(fileID,'%d %d', [2 Inf]);
data6= data6';
offset = data6(1,1);
data6(:,1) = data6(:,1) - offset;
data6(:,1) = data6(:,1)/1000;
plot(data6(:,1),data6(:,2));
fclose(fileID);
title('TxPower=-12dBm');
annotation(figure1,'ellipse',...
    [0.813151270923745 0.348502994011976 0.0405375077495348 0.0491017964071856],'LineStyle','--','Color','red');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_-16.log');
data7 = fscanf(fileID,'%d %d', [2 Inf]);
data7= data7';
offset = data7(1,1);
data7(:,1) = data7(:,1) - offset;
data7(:,1) = data7(:,1)/1000;
plot(data7(:,1),data7(:,2));
fclose(fileID);
title('TxPower=-16dBm');
k=k+1;

subplot(h,2,k);
fileID = fopen('better_-20.log');
data8 = fscanf(fileID,'%d %d', [2 Inf]);
data8= data8';
offset = data8(1,1);
data8(:,1) = data8(:,1) - offset;
data8(:,1) = data8(:,1)/1000;
plot(data8(:,1),data8(:,2));
fclose(fileID);
title('TxPower=-20dBm');
annotation(figure1,'ellipse',...
    [0.703664921465969 0.0843644544431946 0.208376963350785 0.124859392575928],...
    'LineStyle','--','Color','red');
k=k+1;

% subplot(h,2,k);
% fileID = fopen('scan_again.log');
% data = fscanf(fileID,'%d %d', [2 Inf]);
% data = data';
% offset = data(1,1);
% data(:,1) = data(:,1) - offset;
% data(:,1) = data(:,1)/1000;
% plot(data(:,1),data(:,2));
% fclose(fileID);
% xlabel('Time (s)');
% ylabel('RSSI');
% title('TxPower=-8dBm, ScanTxPower=-8dBm');

% Adding axis labels etc.
for i=1:8
    subplot(h,2,i);
    xlabel('Time (s)');
    ylabel('RSSI');
    xlim([0 72]);
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