clear, close all;

% NB Video is 60fps, IMU is 148.148148 fps

% Declare all the captures, their names, and the offset, in seconds, between
% the start of the video and the start of the IMU data.
captures = struct( ...
    "Normal_7_5", struct("name", "Normal_7_5", "offset", 31.625), ...
    "Normal_10", struct("name", "Normal_10", "offset", 32.625), ...
    "Normal_12_5", struct("name", "Normal_12_5", "offset", 30.895), ...
    "Normal_15", struct("name", "Normal_15", "offset", 31), ...
    "Horizontal_7_5", struct("name", "Horizontal_7_5", "offset", 31), ...
    "Horizontal_10", struct("name", "Horizontal_10", "offset", 31), ...
    "Vertical_12_5", struct("name", "Vertical_12_5", "offset", 31), ...
    "Vertical_15", struct("name", "Vertical_15", "offset", 31) ...
);

% Select a capture to work with.
capture = captures.Normal_12_5;

% Sensor to use
% 1: trunk front
% 2: shank left
% 3: shank right (useful for syncing data and video)
sensorID = 3;
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
lookaround = 50;

% Sample rate
Fs = imuSampleRate;

% Offset the start of the IMU data read.
% This will be at least [lookaround] samples at the sample rate, to prevent 
% reading indices outside of the bounds of the IMU data vector.
% Second parameter to max() can be used to set an arbitrary offset.
imuStartTime = max(lookaround/Fs, 78);

imuDuration = imuSamplePeriod*length(imuSamples);

% Read the video.
v = VideoReader(sprintf('%s_480.mov', capture.name));
% Set the video current time via the capture offset.
vidStartTime = imuStartTime + capture.offset;
v.CurrentTime = vidStartTime;

videoFramePeriod = 1/v.FrameRate;
% Time vector for the video frames
vidTvec = linspace(0, v.NumFrames*videoFramePeriod, v.NumFrames)';


Nvid = floor(linspace(1, v.NumFrames, v.Duration*Fs)');
Nimu = floor(linspace(1, length(imuSamples), imuDuration*Fs)');
Tvid = linspace(0, v.Duration, v.Duration*Fs)';
Timu = linspace(0, imuDuration, imuDuration*Fs)';
vidVec = [Nvid, Tvid];
imuVec = [Nimu, Timu];

gyroY = sprintf(gyroID, sensorID, 'Y');
accelY = sprintf(accelID, sensorID, 'Y');
accelX = sprintf(accelID, sensorID, 'X');

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

frame = readFrame(v);
% Try to align the video to the start of the IMU data...
while v.CurrentTime < vidStartTime
    frame = readFrame(v);
end
vidNOffset = 0;
while Tvid(vidNOffset + 1) < v.CurrentTime
    vidNOffset = vidNOffset + 1;
end
imuNOffset = 0;
while Timu(imuNOffset + 1) < imuStartTime
    imuNOffset = imuNOffset + 1;
end

% Axis... only useful if doing a general plot. Maybe remove this.
axisToUse = 'X';

figure('Position', [100, 100, 1500, 640]);

for n=1:length(Nimu)-lookaround
    while Tvid(n + vidNOffset) > v.CurrentTime
        frame = readFrame(v);
    end
    
    % Get range of indices to plot.
    nRange = n + imuNOffset - lookaround:n + imuNOffset + lookaround;
    % Get corresponding times.
    times = Timu(nRange);
    % Get the range of times.
    tRange = [times(1), times(end)];
    
    % Draw the current video frame.
    subplot(131), ...
        imshow(frame), ...
        title(sprintf("t = %f", v.CurrentTime - (vidStartTime - imuStartTime)));
    
    % Plot a frame of gyroscope Y against time
    subplot(132), ...
        plot( ...
            Timu(nRange), ...
            T.(gyroY)(Nimu(nRange)) ...
        ), ...
        % Also plot a marker at the current time
        hold on, ...
        plot(...
            Timu(n + imuNOffset), ...
            T.(gyroY)(Nimu(n + imuNOffset)), ...
            'k-o', ...
            'LineWidth', 2, ...
            'MarkerSize', 10 ...
        ), ...
        hold off, ...
        grid on, ...
        title(sprintf("Gyro Y t=%f s", Timu(n + imuNOffset))), ...
        ylim([-200, 200]), xlim(tRange);
    
    % Plot a frame of accelerometer Y against time
    subplot(133), ...
        plot( ...
            Timu(nRange), ...
            T.(accelY)(Nimu(nRange)) ...
        ), ...
        hold on, ...
        plot(...
            Timu(n + imuNOffset), ...
            T.(accelY)(Nimu(n + imuNOffset)), ...
            'k-o', ...
            'LineWidth', 2, ...
            'MarkerSize', 10 ...
        ), ...
        hold off, ...
        grid on, ...
        title(sprintf("Accel Y t=%f s", Timu(n + imuNOffset))), ...
        ylim([-4, 4]), xlim(tRange);
    

    
%     subplot(132), ...
%         plot(...
%             T.(fieldY)(Nimu(n + imuNOffset)), ...
%             'k-o', ...
%             'LineWidth', 2, ...
%             'MarkerSize', 10 ...
%         ), ...
%         title(sprintf("%s %s t=%f s", plotTitle, axisToUse, Timu(n + imuNOffset))), ...
%         ylim([-lim, lim]);
        
%     subplot(133), ...
%         plot3(...
%             T.(fieldX)(n + imuNOffset), ...
%             T.(fieldY)(n + imuNOffset), ...
%             T.(fieldZ)(n + imuNOffset), ...
%             'k-o', ...
%             'LineWidth', 2, ...
%             'MarkerSize', 10 ...
%         ), ...
%         xlim([-lim, lim]), ylim([-lim, lim]), zlim([-lim, lim]),...
%         xlabel('X'), ylabel('Y'), zlabel('Z'), ...
%         title(sprintf("%s t=%f s", plotTitle, Timu(n + imuNOffset))), ...
% %         view(90, 0);
%         view(45 + n/10, 30);
    

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
range = 5000:5100;
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