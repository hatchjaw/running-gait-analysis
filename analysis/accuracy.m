clear, close all;

captures = struct( ...
    "Normal_7_5", struct("name", "Normal_7_5", "offset", 31.625), ...
    "Normal_10", struct("name", "Normal_10", "offset", 32.625), ...
    "Normal_12_5", struct("name", "Normal_12_5", "offset", 30.895), ...
    "Normal_15", struct("name", "Normal_15", "offset", 31.045), ...
    "Horizontal_7_5", struct("name", "Horizontal_7_5", "offset", 31.625), ...
    "Horizontal_10", struct("name", "Horizontal_10", "offset", 30.575), ...
    "Vertical_12_5", struct("name", "Vertical_12_5", "offset", 30.475), ...
    "Vertical_15", struct("name", "Vertical_15", "offset", 29.6) ...
);

% Select a capture to work with.
capture = captures.Normal_15;

% Sensor to use
% 1: trunk front
% 2: shank left
% 3: shank right (useful for syncing data and video)
sensorID = 1;
% 1: accelerometer
% 2: gyroscope
dataToUse = 1;

% Read the IMU data
T = readtable(sprintf('../captures/%s.csv', capture.name), ...
    'HeaderLines', 214, ...
    'Delimiter', ',' ...
);

% IMU sample period taken from the data file (1/148.148...)
imuSamplePeriod = .00675;
imuSampleRate = 1/imuSamplePeriod;

% Get the indices of the IMU samples that aren't NaN's
imuSamples = rmmissing(T.X_samples__1);

% Placeholder strings for the data IDs
accelID = 'TrignoIMSensor%1$d_Acc%1$d_%2$c_IM__g_';
gyroID = 'TrignoIMSensor%1$d_Gyro%1$d_%2$c_IM__deg_sec_';

% Number of samples to plot around the current time.
lookaround = 75;
% Minimum time interval between a toe-off and the following initial contact.
TOICInterval = .075;
% (Negative) jerk threshold for initial contact detection.
ICjerkThresh = -12.5;
% Acceleration threshold for initial contact detection.
ICaccelThresh = -1;
% Based one the above, IC happened this many samples ago:
IClookbackSamps = 4;
% Acceleration range in which to detect a stance phase reversal.
stancePhaseReversalWindow = [-1.2, -.5];
shankAccelThresh = 3;

% Sample rate
Fs = imuSampleRate;

% Threshold for 'ground truth' IC detection.
switch capture.name
    case "Normal_15"
        shankJerkThresh = 650;
    case "Normal_12_5"
        shankJerkThresh = 500;
    case "Vertical_12_5"
        shankJerkThresh = 500;
    case "Horizontal_7_5"
        shankJerkThresh = 100;
    otherwise
        shankJerkThresh = 225;
end

% Number of strides (pairs of steps) to use for GTC balance running average.
strideLookback = 4;

imuStartTime = max(lookaround/Fs, 0);

imuDuration = imuSamplePeriod*length(imuSamples);

Nimu = floor(linspace(1, length(imuSamples), imuDuration*Fs)');
Timu = linspace(0, imuDuration, imuDuration*Fs)';

% Compute the data IDs for the Y axes of the gyro and accelerometer.
gyroY = T.(sprintf(gyroID, sensorID, 'Y'));
gyroX = T.(sprintf(gyroID, sensorID, 'X'));
gyroZ = T.(sprintf(gyroID, sensorID, 'Z'));
accelY = T.(sprintf(accelID, sensorID, 'Y'));
accelX = T.(sprintf(accelID, sensorID, 'X'));
accelZ = T.(sprintf(accelID, sensorID, 'Z'));
% Shank accel data for ground truth
shankLAccelX = T.(sprintf(accelID, 2, 'X'));
shankRAccelX = T.(sprintf(accelID, 3, 'X'));

% Gyroscope Y filter, for L/R detection.
gyfp = 2.5/(imuSampleRate/2);
gyfs = 50/(imuSampleRate/2);
gyfatt = 60;
[gyfo, gyfc] = buttord(gyfp, gyfs, 3, gyfatt);
[gyfb, gyfa] = butter(gyfo, gyfc);
gyroYFiltered = filter(gyfb, gyfa, gyroY);

% Align the IMU sample index with the IMU start time.
imuNOffset = 0;
while Timu(imuNOffset + 1) < imuStartTime
    imuNOffset = imuNOffset + 1;
end

% Derivative of accelerometer Y -- jerk
jerk = zeros(3, 1);
% Toe-offs
TOs = zeros(1, 4);
% Initial contacts
ICs = zeros(1, 4);
gtICs = zeros(1, 4);
% Ground contact times
GCTs = zeros(1, 2);
gtGCTs = zeros(1, 2);
isStancePhaseReversal = false;
isSwingPhaseReversal = false;
lastLocalMinimum = 0;

canSwapFeet = true;

for n=1:length(Timu)-imuNOffset-lookaround    
    imuCurrentN = n+imuNOffset;
    currentT = Timu(imuCurrentN);
    % Get range of indices to plot.
    nRange = imuCurrentN - lookaround:imuCurrentN + lookaround;
    % Get corresponding times.
    tRange = Timu(nRange);
    
    % Ground truth from the shanks
    shankL = shankLAccelX(Nimu(nRange));
    shankR = shankRAccelX(Nimu(nRange));
    
    % Update jerk
    jerk(3) = jerk(2);
    jerk(2) = jerk(1);
    jerk(1) = (accelY(Nimu(imuCurrentN)) - accelY(Nimu(imuCurrentN-1))) / ...
        imuSamplePeriod;
    
    % Detect most recent local minimum
    if isInflection(jerk, 'min')
        lastLocalMinimum = accelY(Nimu(imuCurrentN));
    end

    % Detect stance phase reversal -- approaching a toe-off
    if ~isStancePhaseReversal && sign(jerk(1)) == 1 && ...
            accelY(Nimu(imuCurrentN)) < stancePhaseReversalWindow(2) && ...
            accelY(Nimu(imuCurrentN)) > stancePhaseReversalWindow(1) && ...
            lastLocalMinimum < -1.5
        isStancePhaseReversal = true;
    end

    % Detect toe-off as jerk local maximum, via change from pos to neg. 
    % Include sign of gyro for detecting L vs R foot.
    if isInflection(jerk, 'max') && isStancePhaseReversal
        % Make sure it's not the same foot again... (1 = L, -1 = R);
        prevFoot = TOs(end, 3);
        nextFoot = sign(gyroYFiltered(Nimu(imuCurrentN-1)));
        if nextFoot == prevFoot
            if canSwapFeet
                nextFoot = -nextFoot;
                canSwapFeet = false;
            else
                canSwapFeet = true;
            end
        end
        TOs(end+1, :) = [...
            Timu(imuCurrentN-1); ...
            accelY(Nimu(imuCurrentN-1)); ...
            nextFoot; ...
            Timu(imuCurrentN-1) - TOs(end, 1)...
        ];

        isStancePhaseReversal = false;
        isSwingPhaseReversal = true;
    
        % Toe off marks the end of a ground contact, so register a ground
        % contact time if possible. Should be greater than zero and (probably)
        % less than three-quarters of a second.
        groundContactTime = TOs(end, 1) - ICs(end, 1);
        gtGCT = TOs(end, 1) - gtICs(end, 1);
        if groundContactTime > 0 && groundContactTime < .75
            GCTs(end+1, :) = [groundContactTime; TOs(end, 3)];
            gtGCTs(end+1, :) = [gtGCT, TOs(end, 3)];
            
            % Calculate GTC balance.
            if size(GCTs, 1) > 1
                if size(GCTs, 1) > 2*strideLookback-1
                    gcts = GCTs(end-(2*strideLookback-1):end, :);
                    gtgcts = gtGCTs(end-(2*strideLookback-1):end, :);
                    L = mean(gcts(gcts(:, 2) == 1, 1));
                    gtL = mean(gtgcts(gtgcts(:, 2) == 1, 1));
                    R = mean(gcts(gcts(:, 2) == -1, 1));
                    gtR = mean(gtgcts(gtgcts(:, 2) == -1, 1));
                else 
                    L = mean(GCTs(GCTs(:, 2) == 1, 1));
                    gtL = mean(gtGCTs(gtGCTs(:, 2) == 1, 1));
                    R = mean(GCTs(GCTs(:, 2) == -1, 1));
                    gtR = mean(gtGCTs(gtGCTs(:, 2) == -1, 1));
                end
                BL = L/(L+R);
                gtBL = gtL/(gtL+gtR);
                BR = R/(L+R);
                gtBR = gtR/(gtL+gtR);
            end
        end
    % Detect initial contact. First high negative jerk event an arbitrary
    % interval after last toe off.
    elseif jerk(1) < ICjerkThresh && ...
            isSwingPhaseReversal && ...
            accelY(Nimu(imuCurrentN)) < ICaccelThresh && ...
            Timu(imuCurrentN-1) - TOs(end, 1) > TOICInterval
        ICs(end+1, :) = [...
            Timu(imuCurrentN-IClookbackSamps); ...
            accelY(Nimu(imuCurrentN-IClookbackSamps)); ...
            -TOs(end, 3); ... % Opposite polarity/foot wrt to last toe off
            Timu(imuCurrentN-IClookbackSamps) - ICs(end, 1) ...
        ];
        isSwingPhaseReversal = false;
%         isStancePhaseReversal = true;
    end

    % Once there's at least one toe-off, start collecting ground-truth ICs
    if size(TOs, 1) > 1
        prevFoot = gtICs(end, 3);
        shankLJerk = ( ...
            shankLAccelX(Nimu(imuCurrentN)) - shankLAccelX(Nimu(imuCurrentN-1))...
        ) / imuSamplePeriod;
        shankRJerk = ( ...
            shankRAccelX(Nimu(imuCurrentN)) - shankRAccelX(Nimu(imuCurrentN-1))...
        ) / imuSamplePeriod;
        if prevFoot ~= 1 && ...
                shankLJerk > shankJerkThresh && ...
                Timu(imuCurrentN) - TOs(end, 1) > TOICInterval
            % Left IC
            gtICs(end+1, :) = [...
                Timu(imuCurrentN - 1); ...
                .25*shankLAccelX(Nimu(imuCurrentN - 1)); ...
                1; ...
                Timu(imuCurrentN - 1) - gtICs(end, 1) ...
            ];
        elseif prevFoot ~= -1 && ...
                shankRJerk < -shankJerkThresh && ...
                Timu(imuCurrentN) - TOs(end, 1) > TOICInterval
            % Right IC
            gtICs(end+1, :) = [...
                Timu(imuCurrentN - 1); ...
                .25*shankRAccelX(Nimu(imuCurrentN - 1)); ...
                -1; ...
                Timu(imuCurrentN - 1) - gtICs(end, 1) ...
            ];
        end
    end
end


% Bland-Altman plot of difference between trunk ICs and shank ICs.
scatterRange = 4:length(ICs)-10;
icDeltas = gtICs(scatterRange, 1) - ICs(scatterRange, 1);
meanDelta = mean(icDeltas);
stdDev = std(icDeltas);
stdErr = stdDev / sqrt(length(icDeltas));
conf = stdErr * 1.96;
pd = fitdist(icDeltas, 'Normal');
ci = paramci(pd);

figure();
scatter(gtICs(scatterRange, 1), icDeltas), ...
    ylim([min(icDeltas) - .01, max(icDeltas) + .01]), ...
%     ylim([-.02, .02]), ...
    xlabel('Time (s)', 'FontSize', 18, 'FontName', 'Times'), ...
    ylabel('Difference (Shank sensor - Trunk sensor)', 'FontSize', 18, 'FontName', 'Times');
hold on;
yline(meanDelta, 'r');
yline(ci(1,1), '--b');
yline(ci(2,1), '--b');
% yline(meanDelta + stdDev, 'b');
% yline(meanDelta - stdDev, 'b');
hold off;

meanGCT = mean(GCTs(:, 1));
disp(['Num steps: ', num2str(length(icDeltas))]);
disp(['Mean error: ', num2str(meanDelta)]);
disp(['95% confidence: [', num2str(ci(2,1)), ', ', num2str(ci(1,1)), ']']);
disp(['Mean GCT ', num2str(meanGCT)]);
disp(['Mean error wrt. GCT: ', num2str(100 * (meanDelta/meanGCT)), '%']);
