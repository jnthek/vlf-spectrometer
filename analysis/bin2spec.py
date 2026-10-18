#!/usr/bin/env python3
"""
Reads VLF spectra binary file and generates a jpg. Also writes out powers for three VLF stations.
"""

__author__ = "Jishnu N. Thekkeppattu"  

import matplotlib
matplotlib.use('Agg')  # Forces the low-memory non-interactive backend
import matplotlib.pyplot as plt
import numpy as np
from scipy.interpolate import RegularGridInterpolator
import argparse
from PIL import Image

plt.rcParams.update({'font.size': 18})

######################################################################################

def regrid(data, out_x, out_y):
    m = max(data.shape[0], data.shape[1])
    y = np.linspace(0, 1.0/m, data.shape[0])
    x = np.linspace(0, 1.0/m, data.shape[1])
    interpolating_function = RegularGridInterpolator((y, x), data)

    yv, xv = np.meshgrid(np.linspace(0, 1.0/m, out_y), np.linspace(0, 1.0/m, out_x))

    return interpolating_function((xv, yv))

def get_chans(freq, req_freq, req_bw):
    f_break_low  = req_freq-(req_bw/2)
    f_break_high = req_freq+(req_bw/2)
    return np.where((freq>f_break_low) & (freq<f_break_high))[0]

######################################################################################

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('-fname', metavar='fname', help='Input filename, no defaults')
args = parser.parse_args()
bin_file = str(args.fname)
print ("Reading "+str(bin_file), flush=True)

metadata = np.fromfile(bin_file, dtype=np.float64, count=128)
epoch = metadata[0]
fs = int(metadata[1])
NFFT = int(metadata[2])
t_int = metadata[3]
datafmt = int(metadata[4])

print("Binary file data params", flush=True)
print("NFFT = "+str(NFFT), flush=True)
print("t_int = "+str(t_int)+"s", flush=True)
print("fs = "+str(fs), flush=True)

######################################################################################

title_string = "VLF spectrogram recorded on "

######################################################################################

len_spectrum = int(NFFT/2)+1+1
if datafmt==0:
    rawdata = np.fromfile(bin_file, dtype=np.float64, offset=1024)
else:
    rawdata = np.fromfile(bin_file, dtype=np.float32, offset=1024)

N_good_spectra = int(len(rawdata)/len_spectrum) #The idea here is to avoid the last corrupted spectrum, if any, due to a sudden exit in the acq code
data_w_timestamps = np.reshape(rawdata[0:int(N_good_spectra*len_spectrum)], (N_good_spectra, len_spectrum))
timestamps = epoch+np.asarray(data_w_timestamps[:,0], dtype=np.float64)
spectra = data_w_timestamps[:,1:]

rec_date = bin_file.split("/")[-1].split("-")[0]

timespan_hrs = (timestamps[-1] - timestamps[0])/(60*60)
ut_begin = (timestamps[0]-int(timestamps[0]/86400)*86400)/(60*60)

rec_year = rec_date[0:4]
rec_month = rec_date[4:6]
rec_day = rec_date[6:8]
freq = np.linspace(0,fs/2,int(NFFT/2)+1) #*(1+(1.679)/100) #The fudge factor was based on DSO based freq test conducted on June 1, 2025

save_tstamps = np.array([])
save_powers_182 = np.array([])
save_powers_198 = np.array([])
save_powers_222 = np.array([])

aver_len = int(60/t_int) #to average for 60 seconds
for i in range(0, len(timestamps), aver_len):
    save_tstamps = np.append(save_tstamps, np.mean(timestamps[i:i+aver_len]))
    save_powers_182 = np.append(save_powers_182, np.mean(spectra[i:i+aver_len,get_chans(freq, 18.25e3, 300)]))
    save_powers_198 = np.append(save_powers_198, np.mean(spectra[i:i+aver_len,get_chans(freq, 19.85e3, 300)]))
    save_powers_222 = np.append(save_powers_222, np.mean(spectra[i:i+aver_len,get_chans(freq, 22.25e3, 300)]))

np.save("vlfpowers", np.array(np.column_stack([save_tstamps, save_powers_182, save_powers_198, save_powers_222]), dtype=np.float32))

spectra_resized = regrid(spectra.T, 270, 720)
del(spectra)

######################################################################################

plt.figure(figsize=(16,9))
dmap_freq = np.linspace(0,fs/2,270)
dmap_time = np.linspace(ut_begin,ut_begin+timespan_hrs,720)
plt.title(title_string+rec_day+"-"+rec_month+"-"+rec_year)
plt.pcolormesh(dmap_time, dmap_freq/1e3, np.log(spectra_resized), cmap='plasma')
plt.ylabel("FREQUENCY (kHz)")
plt.xlabel("Time (hrs UT)")
plt.savefig("vlfspec"+str(rec_date)+".jpg", format="jpg", dpi=100, bbox_inches='tight')
plt.close()
print ("JPG generated and saved", flush=True)
#plt.show()
