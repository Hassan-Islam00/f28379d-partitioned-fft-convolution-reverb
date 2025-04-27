% Read the original audio signals
[reverbSamples, originalFs] = audioread("emt_140_dark_3.wav");

% Convert stereo to mono by averaging the channels
if size(reverbSamples, 2) == 2 % Check if the signal is stereo
    reverbSamples = mean(reverbSamples, 2); % Take the average of left and right channels
end

% Target sampling rate
targetFs = 96000;

% Resample the audio to 96 kHz
reverbSamples = resample(reverbSamples, targetFs, originalFs);

% Take only the first quarter of reverbSamples
reverbSamples = reverbSamples(1:6144, :);

% Split reverb into m-sized segments
m = 512; % Reverb IR segment size
n = 512; % Zero-padding length, same as segment size
numofsegments = floor(length(reverbSamples) / m); % Number of reverb segments

% Save to a .h file
fileID = fopen('IR_FFT3.h', 'w');

for i = 1:numofsegments
    % Select current buffer from all samples
    reverbBuffer = reverbSamples((i - 1) * m + 1:i * m, :);
    reverbBuffer = [reverbBuffer; zeros(n - 1, 1)]; % Zero padding if needed

    % Compute FFT
    K = fft(reverbBuffer);

    % Separate real and imaginary parts
    real_part = real(K); % Extract real parts
    imag_part = imag(K); % Extract imaginary parts

    % Interleave real and imaginary parts
    IR_interleaved = reshape([real_part, imag_part].', [], 1);

    % Generate a C-style format: "1,2,2,-3"
    c_format = sprintf('%.1f,', IR_interleaved); % Round values for float output
    c_format = c_format(1:end - 1); % Remove the trailing comma    

    % Write the result to the file with the desired format
    fprintf(fileID, '#pragma DATA_SECTION(IR_FFT_%d, ".IR_DATA%d");\n', i, i);
    fprintf(fileID, 'const float IR_FFT_%d[2048] = {%s};\n\n', i, c_format); % Save as const float array
end

% Define empty float arrays (renamed to Sample_FFT_X) initialized to zero
for j = 1:12
    fprintf(fileID, '#pragma DATA_SECTION(Sample_FFT_%d, ".IR_DATA%d");\n', j, j);
    fprintf(fileID, 'const float Sample_FFT_%d[2048] = {0};\n\n', j);
end

fclose(fileID);

% Display message
disp('C-style arrays saved to IR_FFT3.h');
