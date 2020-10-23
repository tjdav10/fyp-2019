clear all; close all; clc;
% all from D001 perspective

% D002 -63 (89) Complete: 0 Converged: 1 Close: 1 17:03 74
time = 0:89;
log(1:16) = 0;
log(17:90) = 1;
actual(1:16) = 0;
actual(17:86) = 1;
actual(87:90)= 0;

simple = figure('Name','D001 detecting D002');
subplot(1,2,1)
plot(time, log,'-');
yticks([0 1]);
yticklabels({'Not in proximity', 'In proximity'});
ylim([-0.1 1.1]);
xlim([0 89]);
title('Gathered Data');
xlabel('Time (s)');
subplot(1,2,2)
plot(time, actual,'-r');
yticks([0 1]);
yticklabels({'Not in proximity', 'In proximity'});
ylim([-0.1 1.1]);
xlim([0 89]);
xlabel('Time (s)');
title('Ground Truth');

% % threshold -75
% % D003 -77 (146) Complete: 0 Converged: 1 Close: 0 0:00 0
% % 15s converge then 30s out then 30s then 30 out then 30 in
% log_miss(1:136) = 0;
% time = 0:135;
% actual_miss(1:16) = 0;
% actual_miss(17:46) = 0;
% actual_miss(47:77) = 1;
% actual_miss(77:106) = 0;
% actual_miss(107:136) = 1;
% 
% missed = figure('Name', 'Missed Interaction');
% subplot(1,2,1)
% plot(time, log_miss,'-');
% yticks([0 1]);
% yticklabels({'Not in proximity', 'In proximity'});
% ylim([-0.1 1.1]);
% xlim([0 135]);
% title('Gathered Data');
% xlabel('Time (s)');
% subplot(1,2,2)
% plot(time, actual_miss,'-r');
% yticks([0 1]);
% yticklabels({'Not in proximity', 'In proximity'});
% ylim([-0.1 1.1]);
% xlim([0 135]);
% xlabel('Time (s)');
% title('Ground Truth');

% % too sensitive (threshold -77)
% % D003 -70 (138) Complete: 0 Converged: 1 Close: 1 18:12 123
% log_high(1:16) = 0;
% log_high(17:136) = 1;
% time = 0:135;
% actual_high(1:16) = 0;
% actual_high(17:46) = 0;
% actual_high(47:77) = 1;
% actual_high(77:106) = 0;
% actual_high(107:136) = 1;
% 
% too_high = figure('Name', 'RSSI Threshold Too How');
% subplot(1,2,1)
% plot(time, log_high,'-');
% yticks([0 1]);
% yticklabels({'Not in proximity', 'In proximity'});
% ylim([-0.1 1.1]);
% xlim([0 135]);
% title('Gathered Data');
% xlabel('Time (s)');
% subplot(1,2,2)
% plot(time, actual_high,'-r');
% yticks([0 1]);
% yticklabels({'Not in proximity', 'In proximity'});
% ylim([-0.1 1.1]);
% xlim([0 135]);
% xlabel('Time (s)');
% title('Ground Truth');

% threshold -76, 2m and 1m
% this demomnstrates that it tracks cumulative time
% as long as the device is still within range but not required proximity
% % too sensitive (threshold -77)
% D003 -72 (138) Complete: 0 Converged: 1 Close: 1 18:22 58
log_easy(1:16) = 0;
log_easy(17:47) = 0;
log_easy(48:106) = 1;
log_easy(107:136) = 0;

time = 0:135;
actual_easy(1:16) = 0;
actual_easy(17:46) = 0;
actual_easy(47:77) = 1;
actual_easy(77:106) = 0;
actual_easy(107:136) = 1;

too_easy = figure('Name', 'RSSI Threshold Too How');
subplot(1,2,1)
plot(time, log_easy,'-');
yticks([0 1]);
yticklabels({'Not in proximity', 'In proximity'});
ylim([-0.1 1.1]);
xlim([0 135]);
title('Gathered Data');
xlabel('Time (s)');
subplot(1,2,2)
plot(time, actual_easy,'-r');
yticks([0 1]);
yticklabels({'Not in proximity', 'In proximity'});
ylim([-0.1 1.1]);
xlim([0 135]);
xlabel('Time (s)');
title('Ground Truth');