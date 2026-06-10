import numpy as np
import matplotlib.pyplot as plt

def my_fft(x):

    N = len(x)
    if N <= 1:
        return x
    
    
    even = my_fft(x[0::2])
    odd =  my_fft(x[1::2])
    

    T = [np.exp(-2j * np.pi * k / N) * odd[k] for k in range(N // 2)]
    
    return [even[k] + T[k] for k in range(N // 2)] + \
           [even[k] - T[k] for k in range(N // 2)]


fs = 512 
t = np.linspace(0, 1, fs, endpoint=False)


signal = np.sin(2 * np.pi * 50 * t) + 0.5 * np.sin(2 * np.pi * 380 * t)


fft_result = my_fft(signal)

frequencies = np.linspace(0, fs, fs, endpoint=False)

# --- Plotting Both Signals ---
plt.figure(figsize=(12, 6))

# Subplot 1: Time Domain (The Input Signal)
plt.subplot(2, 1, 1)
plt.plot(t[:100], signal[:100]) # Zoomed in to show first 100 samples
plt.title("Time Domain: Input Signal (Zoomed)")
plt.xlabel("Time (seconds)")
plt.ylabel("Amplitude")
plt.grid(True)


plt.subplot(2, 1, 2)
plt.plot(frequencies[:fs//2], np.abs(fft_result)[:fs//2], color='red')
plt.title("Frequency Domain: User-defined FFT")
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude")
plt.grid(True)

plt.tight_layout() 
plt.show()