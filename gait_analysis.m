clear, close all;

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
T = readtable(sprintf('%s.csv', capture.name), ...
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

% Number of samples to plot around the current time.
lookaround = 75;
TOICInterval = .075;
ICjerkThresh = -15;
ICaccelThresh = -.25;
stancePhaseReversalWindow = [-1, -.5];

% Sample rate
Fs = imuSampleRate;

% Offset the start of the IMU data read.
% This will be at least [lookaround] samples at the sample rate, to prevent 
% reading indices outside of the bounds of the IMU data vector.
% Second parameter to max() can be used to set an arbitrary offset.
imuStartTime = max(lookaround/Fs, 1.9);

imuDuration = imuSamplePeriod*length(imuSamples);

% Read the video.
v = VideoReader(sprintf('%s_480.mov', capture.name));
% Set the video current time via the capture offset.
vidStartTime = imuStartTime + capture.offset;
v.CurrentTime = vidStartTime;

videoFramePeriod = 1/v.FrameRate;
% Time vector for the video frames
vidTvec = linspace(0, v.NumFrames*videoFramePeriod, v.NumFrames)';

% Create time and sample vectors for the video and IMU data, based one the
% sampling rate.
Nvid = floor(linspace(1, v.NumFrames, v.Duration*Fs)');
Nimu = floor(linspace(1, length(imuSamples), imuDuration*Fs)');
Tvid = linspace(0, v.Duration, v.Duration*Fs)';
Timu = linspace(0, imuDuration, imuDuration*Fs)';
% vidVec = [Nvid, Tvid];
% imuVec = [Nimu, Timu];

% Compute the data IDs for the Y axes of the gyro and accelerometer.
gyroY = sprintf(gyroID, sensorID, 'Y');
accelY = sprintf(accelID, sensorID, 'Y');
% accelX = sprintf(accelID, sensorID, 'X');

filtPass = 2.5/(imuSampleRate/2);
filtStop = 50/(imuSampleRate/2);
filtAttenuation = 60;
[filtOrder, filtCutoff] = buttord(filtPass, filtStop, 3, filtAttenuation);
[filtB, filtA] = butter(filtOrder, filtCutoff);
gyroYFiltered = filter(filtB, filtA, T.(gyroY));

gyroLim = 250;
accelLim = 5;

% frame = readFrame(v);
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
% Align the IMU sample index with the IMU start time.
imuNOffset = 0;
while Timu(imuNOffset + 1) < imuStartTime
    imuNOffset = imuNOffset + 1;
end

% Axis... only useful if doing a general plot. Maybe remove this.
axisToUse = 'X';

% Derivative of accelerometer Y -- jerk
jerk = zeros(2, 1);
TOs = zeros(1, 4);
ICs = zeros(1, 4);
GCTs = zeros(1, 2);
isStancePhaseReversal = false;
isSwingPhaseReversal = false;
gaitPhase = GaitEvent.Unknown;

% Create a figure for the plot of the video, plus gyro & accelerometer Y axes.
figure('Position', [100, 100, 1500, 640], 'Name', capture.name);

for n=1:length(Nimu)-lookaround
% for n=1:1000
    % (Maybe) advance the video frame
    while Tvid(n + vidNOffset) > v.CurrentTime
        frame = readFrame(v);
    end
    
    imuCurrentN = n+imuNOffset;
    % Get range of indices to plot.
    nRange = imuCurrentN - lookaround:imuCurrentN + lookaround;
    % Get corresponding times.
    times = Timu(nRange);
    % Get the range of times.
    tRange = [times(1), times(end)];
    
    % Update jerk
    jerk(2) = jerk(1);
    jerk(1) = (T.(accelY)(Nimu(imuCurrentN)) - T.(accelY)(Nimu(imuCurrentN-1))) / imuSamplePeriod;
    % Detect stance phase reversal
    if ~isStancePhaseReversal && sign(jerk(1)) == 1 && ...
            T.(accelY)(Nimu(imuCurrentN)) < stancePhaseReversalWindow(2) && ...
            T.(accelY)(Nimu(imuCurrentN)) > stancePhaseReversalWindow(1)
        isStancePhaseReversal = true;
    end
    % Detect toe-off via change in jerk sign from pos to neg. 
    % Include sign of gyro for detecting L vs R foot.
    if sign(jerk(1)) == -1 && ...
            sign(jerk(2)) == 1 && ...
            isStancePhaseReversal
        TOs(end+1, :) = [...
            Timu(imuCurrentN-1); ...
            T.(accelY)(Nimu(imuCurrentN-1)); ...
            sign(T.(gyroY)(Nimu(imuCurrentN-1))); ...
            Timu(imuCurrentN-1) - TOs(end, 1)...
        ];
    
        groundContactTime = TOs(end, 1) - ICs(end, 1);
        if groundContactTime > 0 && groundContactTime < .75
            GCTs(end+1, :) = [groundContactTime; TOs(end, 3)];
        end
    
        isStancePhaseReversal = false;
        isSwingPhaseReversal = true;
    % Detect initial contact. First high negative jerk event an arbitrary
    % interval after last toe off.
    elseif jerk(1) < ICjerkThresh && ...
            isSwingPhaseReversal && ...
            T.(accelY)(Nimu(imuCurrentN)) < ICaccelThresh && ...
            Timu(imuCurrentN-1) - TOs(end, 1) > TOICInterval
        ICs(end+1, :) = [...
            Timu(imuCurrentN-1); ...
            T.(accelY)(Nimu(imuCurrentN-1)); ...
            -TOs(end, 3); ... % Opposite polarity/foot wrt to last toe off
            Timu(imuCurrentN-1) - ICs(end, 1) ...
        ];
        isSwingPhaseReversal = false;
%         isStancePhaseReversal = true;
    end
    % Filter toe-offs and initial contacts outside range
    if size(TOs, 1) > 10
        TOs = TOs(2:end, :);
        if isempty(TOs)
            TOs = zeros(1, 4);
        end
    end
    if size(ICs, 1) > 10
        ICs = ICs(2:end, :);
        if isempty(ICs)
            ICs = zeros(1, 4);
        end
    end
    
    % Draw the current video frame.
    subplot(131), ...
        imshow(frame), ...
        title(sprintf("t = %f", v.CurrentTime - (vidStartTime - imuStartTime)));
    
    % Plot a frame of accelerometer Y against time
    subplot(132), ...
        plot( ...
            Timu(nRange), ...
            T.(accelY)(Nimu(nRange)) ...
        ), ...
        hold on, ...
        plot(...
            Timu(imuCurrentN), ...
            T.(accelY)(Nimu(imuCurrentN)), ...
            'k-o', ...
            'LineWidth', 2, ...
            'MarkerSize', 10 ...
        );
    
    % Indicate toe-offs
    indicateEvents(TOs, "Toe off", 'rx', "ITI", -.4, accelLim - 1, Timu, imuCurrentN, lookaround);
    % Indicate ICs
    indicateEvents(ICs, "Initial contact", 'g+', "ICI", .1, accelLim - 1, Timu, imuCurrentN, lookaround);
    % Ground contact time
    if size(GCTs, 1) > 1
        if size(GCTs, 1) > 9
            gcts = GCTs(end-9:end, :);
            L = mean(gcts(gcts(:, 2) == 1, 1));
            R = mean(gcts(gcts(:, 2) == -1, 1));
        else 
            L = mean(GCTs(GCTs(:, 2) == 1, 1));
            R = mean(GCTs(GCTs(:, 2) == -1, 1));
        end
        BL = L/(L+R);
        BR = R/(L+R);
        text(Timu(imuCurrentN) - .175, ...
            2.5, ...
            { ...
                "$GCT = " + num2str(GCTs(end, 1)) + "$", ...
                "$B_L = " + num2str(BL * 100) + "\%$", ...
                "$B_R = " + num2str(BR * 100) + "\%$"
            }, ...
            'interpreter', 'latex', 'FontSize', 16 ...
        );
    end
    hold off, ...
        grid on, ...
        title(sprintf("Accel Y t=%f s", Timu(imuCurrentN))), ...
        ylim([-accelLim, accelLim]), xlim(tRange);
    
    % Plot a frame of gyroscope Y against time
    subplot(133), ...
        % Raw gyro Y
        plot( ...
            Timu(nRange), ...
            T.(gyroY)(Nimu(nRange)) ...
        ), ...
        hold on, ...
        % Filtered gyro Y
        plot( ...
            Timu(nRange), ...
            gyroYFiltered(Nimu(nRange)), ...
            'LineWidth', 2 ...
        ), ...
        % Signum of filtered gyro Y
        plot( ...
            Timu(nRange), ...
            100*sign(gyroYFiltered(Nimu(nRange))), ...
            'LineWidth', 3 ...
        ), ...
        % Also plot a marker at the current time
        plot(...
            Timu(imuCurrentN), ...
            T.(gyroY)(Nimu(imuCurrentN)), ...
            'k-o', ...
            'LineWidth', 2, ...
            'MarkerSize', 10 ...
        ), ...
        hold off, ...
        grid on, ...
        title(sprintf("Gyro Y t=%f s", Timu(imuCurrentN))), ...
        ylim([-gyroLim, gyroLim]), xlim(tRange);
    
    drawnow;
end

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