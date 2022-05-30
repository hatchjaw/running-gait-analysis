clear, close all;

% TODO: sonification: PD/Max/JUCE

% NB Video is 60fps, IMU is 148.148148 fps

% Declare all the captures, their names, and the offset, in seconds, between
% the start of the video and the start of the IMU data.
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
capture = captures.Normal_12_5;

% Sensor to use
% 1: trunk front
% 2: shank left
% 3: shank right (useful for syncing data and video)
sensorID = 1;
% 1: accelerometer
% 2: gyroscope
dataToUse = 1;

% Read the IMU data
T = readtable(sprintf('captures/%s.csv', capture.name), ...
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

%% Video + accelY + gyroY
close all;

doVideo = false;
doPlot = true;
plotGyro = false;
plotAccel = false;
% Number of samples to plot around the current time.
lookaround = 75;
% Minimum time interval between a toe-off and the following initial contact.
TOICInterval = .075;
% (Negative) jerk threshold for initial contact detection.
ICjerkThresh = -12.5;
% Acceleration threshold for initial contact detection.
ICaccelThresh = -1;
% Acceleration range in which to detect a stance phase reversal.
stancePhaseReversalWindow = [-1, -.5];
% % Threshold for 'ground truth' IC detection.
% shankJerkThresh = 350;
shankAccelThresh = 7;
% Number of pairs of strides to use for GTC balance running average.
strideLookback = 4;

% Sample rate
Fs = imuSampleRate;

% Offset the start of the IMU data read.
% This will be at least [lookaround] samples at the sample rate, to prevent 
% reading indices outside of the bounds of the IMU data vector.
% Second parameter to max() can be used to set an arbitrary offset.
imuStartTime = max(lookaround/Fs, 1);

imuDuration = imuSamplePeriod*length(imuSamples);

if doVideo
    % Read the video.
    v = VideoReader(sprintf('videos/%s_480.mov', capture.name));
    % Set the video current time via the capture offset.
    vidStartTime = imuStartTime + capture.offset;
    v.CurrentTime = vidStartTime;
end

% Create time and sample vectors for the video and IMU data, based one the
% sampling rate.
if doVideo
    Nvid = floor(linspace(1, v.NumFrames, v.Duration*Fs)');
    Tvid = linspace(0, v.Duration, v.Duration*Fs)';
end
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
filtPass = 2.5/(imuSampleRate/2);
filtStop = 50/(imuSampleRate/2);
filtAttenuation = 60;
[filtOrder, filtCutoff] = buttord(filtPass, filtStop, 3, filtAttenuation);
[filtB, filtA] = butter(filtOrder, filtCutoff);
gyroYFiltered = filter(filtB, filtA, gyroY);

% Y limits for plots
gyroLim = 250;
accelLim = 5;


if doVideo
    % Align the video with the start of the IMU data...
    % Read video frames until the video current time is greater than or equal to the
    % offset.
    while v.CurrentTime < vidStartTime
        frame = readFrame(v);
    end
    % Align video sample index with the video current time.
    vidNOffset = 0;
    while Tvid(vidNOffset + 1) < v.CurrentTime
        vidNOffset = vidNOffset + 1;
    end
end
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

if doPlot || doVideo
    % Create a figure for the plot of the video, plus gyro & accelerometer Y axes.
    figure('Position', [100, 100, 1500, 750], 'Name', capture.name);
end

for n=1:length(Timu)-imuNOffset-lookaround
% for n=1:1000
    if doVideo
        % (Maybe) advance the video frame
        while Tvid(n + vidNOffset) > v.CurrentTime
            frame = readFrame(v);
        end
    end
    
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
            nextFoot = -nextFoot;
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
                
                if doPlot
                    subplot(236);
                    scatter(BR*100, -.1, 100), ...
                        xlim([45, 55]), ...
                        ylim([-.5, .5]), ...
                        xticks(46:54), ...
                        yticks([]), ...
                        grid on;
                    title("GCT balance, " + num2str(2*strideLookback) + " stride average");
                    hold on;
                    text(50, -.2, ...
                        "Trunk only: $" + num2str(BR * 100) + "\%$", ...
                        'interpreter', 'latex', ...
                        'FontSize', 16 ...
                    );
                    scatter(gtBR*100,.1, 100);
                    text(50, .2, ...
                        "Shank ICs: $" + num2str(gtBR * 100) + "\%$", ...
                        'interpreter', 'latex', ...
                        'FontSize', 16 ...
                    );
                    xline(50);
                    hold off;
                end
            end
        end
    % Detect initial contact. First high negative jerk event an arbitrary
    % interval after last toe off.
    elseif jerk(1) < ICjerkThresh && ...
            isSwingPhaseReversal && ...
            accelY(Nimu(imuCurrentN)) < ICaccelThresh && ...
            Timu(imuCurrentN-1) - TOs(end, 1) > TOICInterval
        ICs(end+1, :) = [...
            Timu(imuCurrentN-4); ...
            accelY(Nimu(imuCurrentN-4)); ...
            -TOs(end, 3); ... % Opposite polarity/foot wrt to last toe off
            Timu(imuCurrentN-4) - ICs(end, 1) ...
        ];
        isSwingPhaseReversal = false;
%         isStancePhaseReversal = true;
    end

    % Once there's at least one toe-off, start collecting ground-truth ICs
    if size(TOs, 1) > 1
        prevFoot = gtICs(end, 3);
        if prevFoot ~= 1 && shankLAccelX(Nimu(imuCurrentN)) < -shankAccelThresh
            % Left IC
            gtICs(end+1, :) = [...
                Timu(imuCurrentN); ...
                .25*shankLAccelX(Nimu(imuCurrentN)); ...
                1; ...
                Timu(imuCurrentN) - gtICs(end, 1) ...
            ];
        elseif prevFoot ~= -1 && shankRAccelX(Nimu(imuCurrentN)) > shankAccelThresh
            % Right IC
            gtICs(end+1, :) = [...
                Timu(imuCurrentN); ...
                .25*shankRAccelX(Nimu(imuCurrentN)); ...
                -1; ...
                Timu(imuCurrentN) - gtICs(end, 1) ...
            ];
        end
    end

    if doVideo
        % Draw the current video frame.
        subplot(2,3,[1,4]), ...
            imshow(frame), ...
            title(sprintf("t = %f", v.CurrentTime - (vidStartTime - imuStartTime)));
    end
    
    if doPlot
        if plotAccel
            % Plot a frame of accelerometer Y against time
            subplot(2,3,[2,5]), ...
                plot( tRange, accelY(Nimu(nRange)), 'LineWidth', 2 ), ...
                hold on, ...
                plot(...
                    currentT, ...
                    accelY(Nimu(imuCurrentN)), ...
                    'k-o', ...
                    'LineWidth', 2, ...
                    'MarkerSize', 10 ...
                );
            % Indicate toe-offs
            indicateEvents(TOs, "Toe off", 'rx', "ITI", -.4, accelLim - 1, Timu, imuCurrentN, lookaround);
            % Indicate ICs
            indicateEvents(ICs, "Initial contact", 'g+', "ICI", .1, accelLim - 1, Timu, imuCurrentN, lookaround);
            % Compare with ground truth...
            indicateEvents(gtICs, "gtIC", 'c*', "gtICI", .1, accelLim - 2.5, Timu, imuCurrentN, lookaround);

            % Display ground contact time and balance.
            if size(GCTs, 1) > 1
                text(currentT - .4, ...
                    2.5, ...
                    { ...
                        "$GCT = " + num2str(GCTs(end, 1)) + "$", ...
                        "$B_L = " + num2str(BL * 100) + "\%$", ...
                        "$B_R = " + num2str(BR * 100) + "\%$"
                    }, ...
                    'interpreter', 'latex', 'FontSize', 16 ...
                );
            end

            plot(tRange, .25*shankL),
            plot(tRange, .25*shankR),

            hold off, ...
                grid on, ...
                title(sprintf("Accel Y t=%f s", currentT)), ...
                ylim([-accelLim, accelLim]), xlim([tRange(1), tRange(end)]);
        end
        
        if plotGyro
            % Plot a frame of gyroscope Y against time
            subplot(233), ...
                % Raw gyro Y
                plot( ...
                    tRange, ...
                    gyroY(Nimu(nRange)) ...
                ), ...
                hold on, ...
                % Filtered gyro Y
                plot( ...
                    tRange, ...
                    gyroYFiltered(Nimu(nRange)), ...
                    'LineWidth', 2 ...
                ), ...
                % Signum of filtered gyro Y
                plot( ...
                    tRange, ...
                    100*sign(gyroYFiltered(Nimu(nRange))), ...
                    'LineWidth', 3 ...
                ), ...
                % Also plot a marker at the current time
                plot(...
                    currentT, ...
                    gyroY(Nimu(imuCurrentN)), ...
                    'k-o', ...
                    'LineWidth', 2, ...
                    'MarkerSize', 10 ...
                ), ...
                hold off, ...
                grid on, ...
                title(sprintf("Gyro Y t=%f s", currentT)), ...
                ylim([-gyroLim, gyroLim]), xlim([tRange(1), tRange(end)]);
        end
        
        drawnow;
    end
end

scatterRange = 4:length(ICs)-20;
icDeltas = gtICs(scatterRange, 1) - ICs(scatterRange, 1);
meanDelta = mean(icDeltas);
stdDev = std(icDeltas);
figure();
scatter(gtICs(scatterRange, 1), icDeltas),title('Bland-Altman');%, ylim([-.1, .1]);
hold on;
yline(meanDelta, 'r');
yline(meanDelta + stdDev, 'b');
yline(meanDelta - stdDev, 'b');
hold off;

%% Animated plots of IMU data

% '2d' or '3d'
plotMode = '3d';

switch dataToUse
    case 2
        dataID = gyroID;
        plotTitle = 'Gyroscope';
        lim = 300;
    otherwise
        dataID = accelID;
        plotTitle = 'Accelerometer';
        lim = 4;
end

fieldX = sprintf(dataID, sensorID, 'X');
fieldY = sprintf(dataID, sensorID, 'Y');
fieldZ = sprintf(dataID, sensorID, 'Z');

switch plotMode
    case '2d'
        increment = 1;
    otherwise
        increment = 2;
end

for i=1:increment:length(T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_X_IM__g_', sensorID)))
    range = i:i+increment;
    switch plotMode
        case '2d'
            subplot(131), ...
                plot(...
                    0, ...
                    T.(sprintf(dataID, sensorID, 'X'))(i), ...
                    'k-o', ...
                    'LineWidth', 2, ...
                    'MarkerSize', 10 ...
                    ), ...
                    title('X'), ...
                    xlim([-lim, lim]), ylim([-lim, lim]);
            subplot(132), ...
                plot(...
                    0, ...
                    T.(sprintf(dataID, sensorID, 'Y'))(i), ...
                    'k-o', ...
                    'LineWidth', 2, ...
                    'MarkerSize', 10 ...
                    ), ...
                    title('Y'), ...
                    xlim([-lim, lim]), ylim([-lim, lim]);
            subplot(133), ...
                plot(...
                    0, ...
                    T.(sprintf(dataID, sensorID, 'Z'))(i), ...
                    'k-o', ...
                    'LineWidth', 2, ...
                    'MarkerSize', 10 ...
                    ), ...
                    title('Z'), ...
                    xlim([-lim, lim]), ylim([-lim, lim]);
            sgtitle(sprintf("%s t=%f s", plotTitle, i*imuSamplePeriod));
        otherwise
            plot3(...
                mean(T.(sprintf(dataID, sensorID, 'X'))(range)), ...
                mean(T.(sprintf(dataID, sensorID, 'Y'))(range)), ...
                mean(T.(sprintf(dataID, sensorID, 'Z'))(range)), ...
                'k-o', ...
                'LineWidth', 2, ...
                'MarkerSize', 10 ...
            ), ...
            xlim([-lim, lim]), ylim([-lim, lim]), zlim([-lim, lim]),...
            xlabel('X'), ylabel('Y'), zlabel('Z'), ...
%             sgtitle(plotTitle), ...
            title(sprintf("%s t=%f s", plotTitle, i*imuSamplePeriod)), ...
%             view(90, 90);
            view(45 + i/10, 30);
    end
    
    drawnow;
end

%% A snapshot of the accelerometer data...
figure;
plot(T.X_samples_, T.(sprintf('TrignoIMSensor%1$d_EMG%1$d_IM__Volts_', sensorID))), ...
    title('EMG');

figure;
range = 4000:4100;
plot3(...
    T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_X_IM__g_', sensorID))(range), ...
    T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_Y_IM__g_', sensorID))(range), ...
    T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_Z_IM__g_', sensorID))(range) ...
), title('Accelerometer');

%% Some sensor plots vs. time

% Vector of timestamps for the IMU samples.
imuTvec = linspace(0, length(imuSamples)*imuSamplePeriod, length(imuSamples))';

% Plot accelerometer data against time
dAcc = [...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_X_IM__g_', sensorID))), ...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_Y_IM__g_', sensorID))), ...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_Z_IM__g_', sensorID))) ...
];
figure;
plot(imuTvec, dAcc), ...
    xlabel('Time (s)'), ...
    title('Accelerometer'), ...
    legend('x (lateral)', 'y (vertical)', 'z (anterior-posterior)');

plot(imuTvec,  rmmissing(T.(sprintf('TrignoIMSensor%1$d_Acc%1$d_X_IM__g_', 2))));

% Plot gyroscope data against time
dGyro = [...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Gyro%1$d_X_IM__deg_sec_', sensorID))), ...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Gyro%1$d_Y_IM__deg_sec_', sensorID))), ...
    rmmissing(T.(sprintf('TrignoIMSensor%1$d_Gyro%1$d_Z_IM__deg_sec_', sensorID))) ...
];
figure;
plot(imuTvec, dGyro), ...
    xlabel('Time (s)'), ...
    title('Gyroscope'), ...
    legend('x (pitch)', 'y (yaw)', 'z (roll)');