close all;

% Setup & Signal Generation
frow = [697, 770, 852, 941];
fcol = [1209, 1336, 1477, 1633];
sym = ['1', '4', '7', '*', '2', '5', '8', '0', '3', '6', '9', '#', 'A', 'B', 'C', 'D'];
symrow = [1 2 3 4 1 2 3 4 1 2 3 4 1 2 3 4];
symcol = [1 1 1 1 2 2 2 2 3 3 3 3 4 4 4 4];
symmtx = [1, 5, 9, 13; 2, 6, 10, 14; 3, 7, 11, 15; 4, 8, 12, 16];

Fs = 4000;
N = 200;

% Efficient signal generation using a cell array
x_parts = cell(1, length(sym));
for i = 1:length(sym)
    Nsym = N; %+ round(N/4 * rand(1));
    t = (0:Nsym-1)/Fs;
    x_parts{i} = sin(2*pi*frow(symrow(i))*t) + sin(2*pi*fcol(symcol(i))*t);
end
x = [x_parts{:}];

% 8-bit Quantization
bits = 8;
Q = (max(x) - min(x))/(2^bits);
x = round(x/Q);

% Decoding starts here
fbin = Fs/N;
k = round([frow fcol]/fbin);

% Use buffer to split signal into N-length blocks automatically
x_blocks = buffer(x, N, 0, 'nodelay'); 
x_blocks = x_blocks .* hamming(N); % Apply window to all blocks at once

% Run Goertzel on the entire matrix at once (k+1 for 1-based indexing)
myspec = abs(goertzel(x_blocks, k + 1))'; 

th = max(myspec(:))/2;
myspecbin = myspec > th;
xblocks = size(myspec, 1);
symout = repmat(' ', 1, xblocks);

for i = 1:xblocks
    % searches for row frequency peak: 697-941
    idx1 = find(myspecbin(i, 1:4), 1);
    % searches for col frequency peak: 1209-1633
    idx2 = find(myspecbin(i, 5:8), 1);
    
    if ~isempty(idx1) && ~isempty(idx2)
        symout(i) = sym(symmtx(idx1, idx2));
    end
end

% Visuals (Original Titles)
fprintf('reference string: %s\n', sym); % Simplified for demo
fprintf('decoded string:   %s\n', symout);


figure; 
imagesc((0:xblocks)*N/Fs*1000, fbin*k/1000, myspec'); 
axis xy;
colorbar;
xlabel('Time (ms)'); ylabel('Frequency (kHz)');
title(sprintf('DTMF test, %d-bit Fs=%.1f kHz, %d-point Goertzel', bits, Fs/1000, N));

figure; 
imagesc((0:xblocks)*N/Fs*1000, fbin*k/1000, myspecbin'); 
axis xy;
colorbar;
xlabel('Time (ms)'); ylabel('Frequency (kHz)');
title(sprintf('DTMF test, %d-bit Fs=%.1f kHz, %d-point Goertzel, th=%1.f', bits, Fs/1000, N, th));

figure; 
spectrogram(x, hamming(128), 120, 128, Fs, 'yaxis');
title(sprintf('DTMF test, %d-bit Fs=%.1f kHz, %d-points FFT', bits, Fs/1000, 128));