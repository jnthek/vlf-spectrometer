/**
* @file alsaspectrometer.c
* @author Jishnu N. Thekkeppattu
* @date 2026-10-18
* @brief A spectrometer utility that uses the GNU/linux ALSA framework.
* @details This code can be used to record spectra using a soundcard. It uses FFTW3 to compute spectra, and writes out the spectra as 
* a binary file with some metadata. A description of structure of the output binary file format is below.
* # Binary file format
*   1) Header
*      A binary data file starts with a 1024-byte header, which is restructured into a 128 double array. 
*      The contents of this array, in their order, are as follows
*          a) Epoch :- this is the epoch, in UNIX timestamp format, with respect to which the timestamp offsets are calculated for the spectra.
*          b) Sampling rate :- Sampling frequency in Hz
*          c) NFFT :- self explanatory, the spectra have (NFFT/2)+1 values as the input data are real
*          d) Integration time :- the actual integration time in s, this is generally lower than the desired time entered by the user
*          e) Data precision :- Indicates whether the spectral data are in float32 format. This is based on a macro USEFLOAT set in this C code.
*              If the value is 1, the spectra section has float precision (32 bits). 
*              If the value is 0, the spectra are double precision (64 bits). 
*          f) (to be implemented) :- An eight byte string that has the observatory name. This will be written out by the C code as a char array 
*              and will have to be read back in python as a string.
*      The rest of the 1024 byte array is padded with zeros, and can be used in future for additional metadata. 
*      The reason for doing this way is to enable easy loading into other languages, especially python/numpy 
*      which has only minimal support for binary files. Besides, the entire header can be inspected easily using a single od command.
*   2) Spectra
*      Each spectrum is an (NFFT/2)+2 array, containing a leading timestamp offset (with respect to the epoch given in the header), 
*      and the next (NFFT/2)+1 points constituting the actual VLF spectrum. This structure is repeated with each integration.
*      The timestamp offset specified above is the difference between the acquisition time and the starttime specified in the epoch variable above.
*/

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h> 
#include <time.h>
#include <stdlib.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <sys/mman.h>

#define PI 3.14159265358979323846 
#define USEFLOAT // If this is defined, float32 will be used as the format for data. If not, float64 aka double will be used.
#ifdef USEFLOAT
    #define DATAFMT float
#endif

#ifndef USEFLOAT
    #define DATAFMT double
#endif

void print_usage() 
{
    printf("Usage: alsaspectrometer [options]\n");
    printf("  -D Specify the input sound device (e.g. hw:1,0), no default\n");
    printf("  -N Specify the number of points of FFT (default 4096)\n");
    printf("  -r Specify the ADC sample rate (default 96000)\n");
    printf("  -i Specify the integration time in seconds (default 1). The actual integration time will be different to satisfy certain FFT criteria\n");
    printf("  -T Specify the acquisition duration, also knows as timeout, in seconds (default 60)\n");
    printf("  --help Display this help message\n");
}

void blackman_window(double *window, int N) 
{
    for (int n = 0; n < N; n++) 
    {
        window[n] = 0.42 - 0.5 * cos(2 * PI * n / (N - 1)) + 0.08 * cos(4 * PI * n / (N - 1));
    }
}

int main(int argc, char *argv[])
{
    struct timespec tv;
    unsigned int rate = 96000;
    unsigned int NFFT = 4096;
    unsigned int t_int = 1; //Maybe float can be used for much higher temporal resolution, but in that case, why not record raw .wav files ?
    unsigned int timeout = 60;

    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;

    int opt;
    int err;
    int has_devstring=0;
    static struct option long_options[] = {{"help", no_argument, NULL, 'h'}, {0, 0, 0, 0}}; // Define long options
    while ((opt = getopt_long(argc, argv, "D:r:C:N:i:T:h:?", long_options, NULL)) != -1) 
    {
        switch (opt) 
        {
            case 'D':  // device
                has_devstring = 1;
                if ((err = snd_pcm_open (&capture_handle, optarg, SND_PCM_STREAM_CAPTURE, 0)) < 0) 
                {
                    fprintf (stderr, "cannot open audio device %s (%s)\n", optarg, snd_strerror (err));
                    exit(EXIT_FAILURE);
                }
                break;
            case 'r':  // sampling rate
                rate = (unsigned int) strtol(optarg, NULL, 10);
                break;
            case 'C':  // channels - to be decided later on how to use this information
                // rate = (unsigned int) strtol(optarg, NULL, 10);
                break;
            case 'N':  // NFFT
                NFFT = (unsigned int) strtol(optarg, NULL, 10);
                break;
            case 'i':  // integration time
                t_int = (unsigned int) strtol(optarg, NULL, 10);
                break;
            case 'T':  // timeout
                timeout = (unsigned int) strtol(optarg, NULL, 10);
                break;
            case 'h':  // Help
                print_usage();
                return 0;
            case '?':  // Invalid option
                print_usage();
                return 1;
            default:
                print_usage();
                return 1;
        }
    }
    
    if (!has_devstring) {
		fprintf(stderr, "Error: The required '-D' option was not provided.\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

    snd_pcm_hw_params_malloc (&hw_params);
    snd_pcm_hw_params_any (capture_handle, hw_params);
    snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S32_LE)) < 0) // Will not compromise on this !
    {
        fprintf (stderr, "cannot set sample format (%s)\n",snd_strerror (err));
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_hw_params_set_rate (capture_handle, hw_params, rate, 0)) < 0) 
    {
        fprintf (stderr, "cannot set sample rate (%s)\n",snd_strerror (err));
        exit(EXIT_FAILURE);
    }    
    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) 
    {
        fprintf (stderr, "cannot set channel count (%s)\n",snd_strerror (err));
        exit(EXIT_FAILURE);
    }
    snd_pcm_hw_params (capture_handle, hw_params);
    snd_pcm_hw_params_free (hw_params);
    snd_pcm_prepare (capture_handle);

    char filename[100];
    struct tm *utc_tm;
    unsigned int Nframes;
    float actual_t_int;

    int32_t *ADC_buffer;
    int Naver;
    double start_time;
    double acq_time = 0;
    int Nfftout;

    Nfftout = NFFT/2 + 1;

    double* in_buffer_fft = fftw_alloc_real(NFFT);
    double* bm_window =  malloc(NFFT*sizeof(double));
    blackman_window(bm_window, NFFT);

    fftw_complex *out_buffer_fft = fftw_alloc_complex(Nfftout);
    
    if (0 == fftw_import_wisdom_from_filename("fftw_wisdom.dat")) //Use of FFTW wisdom here, can be problematic if NFFT != 4096 ?  
    {
        fprintf(stderr, "No wisdom file found, it may take some time to start\n");
    } 
    fftw_plan p = fftw_plan_dft_r2c_1d(NFFT, in_buffer_fft, out_buffer_fft, FFTW_MEASURE); 
    fftw_export_wisdom_to_filename("fftw_wisdom.dat");

    Nframes = (int)(t_int*NFFT*((int)(rate/NFFT))); //This logic needs to be revisited ...........
    ADC_buffer = malloc(Nframes * 4 * 2); //Each sample is 32 bits or 4 bytes long and each frame has two samples - one for R and one for L
    if (0==mlock(ADC_buffer, Nframes * 4 * 2)) //memlock the acquisition buffer to avoid paging it
    {
        fprintf(stderr, "ADC buffer successfully locked to memory\n");
    }
    Naver = (int)(Nframes/NFFT);
    actual_t_int = Nframes/(float)rate;

    printf ("%d, %d, %f\n", Nframes, Naver, actual_t_int);

    FILE *file_pointer;
    double* metadata = malloc(1024); //1KB of metadata space, double needed to store the epoch of unixtimestamp
    DATAFMT* avg_power_spec = malloc(Nfftout*sizeof(DATAFMT));
    DATAFMT timedelta = 0;

    clock_gettime(CLOCK_REALTIME, &tv);
    utc_tm = gmtime(&tv.tv_sec);
    strftime(filename, sizeof(filename), "%Y%m%d-%H%M%S_vlf.bin", utc_tm);
    fprintf(stdout, "File is %s \n", filename);
    file_pointer = fopen(filename, "wb");

    for (int i=0; i<(1024/sizeof(sizeof(metadata[0]))); i++)
    {
        metadata[i] = 0;
    }
    clock_gettime(CLOCK_REALTIME, &tv);
    start_time = tv.tv_sec + ((float)tv.tv_nsec)/1e9; 
    metadata[0] = (double)start_time;
    metadata[1] = (double)rate;
    metadata[2] = (double)NFFT;
    metadata[3] = (double)actual_t_int;
    #ifdef USEFLOAT
        metadata[4] = 1.0;
    #endif
    fwrite (metadata, sizeof(metadata[0]), (1024/sizeof(sizeof(metadata[0]))), file_pointer);
    free(metadata);

    while ((acq_time < (start_time + timeout)))
    {
        if ((err = snd_pcm_readi (capture_handle, ADC_buffer, Nframes)) != Nframes) 
        {
            fprintf (stderr, "read from audio interface failed (%s)\n", snd_strerror (err));
            exit(EXIT_FAILURE);
        }

        for (int i=0; i<Nfftout; i++)
        {
            avg_power_spec[i] = 0;
        }

        for (int aver_index=0; aver_index<Naver; aver_index++)
        {            
            for (int i=0; i<NFFT; i++)
            {
                in_buffer_fft[i] = (((double)(ADC_buffer[(aver_index*NFFT) + (i*2) + 1]))/INT32_MAX) * bm_window[i]; //Windowing is applied here
                //Some interference removal can be done here ?
            }
            fftw_execute(p);
            for (int fft_index = 0; fft_index < Nfftout; fft_index++)
            {
                avg_power_spec[fft_index] += (DATAFMT)(out_buffer_fft[fft_index][0]*out_buffer_fft[fft_index][0] + out_buffer_fft[fft_index][1]*out_buffer_fft[fft_index][1]);
            }
        }
        clock_gettime(CLOCK_REALTIME, &tv);
        acq_time = tv.tv_sec + ((float)tv.tv_nsec)/1e9;
        timedelta = (DATAFMT)(acq_time-start_time);
        fprintf(stdout, "Unix timestamp: %f\n",acq_time);
        fwrite (&timedelta, sizeof(timedelta), 1, file_pointer);
        fwrite (avg_power_spec, sizeof(avg_power_spec[0]), Nfftout, file_pointer);
    }
    fprintf(stdout, "Exiting .. \n");
    fftw_destroy_plan(p);
    if (0==munlock(ADC_buffer, Nframes * 4 * 2))
    {
        fprintf(stderr, "ADC buffer successfully unlocked\n");
    }
    free(ADC_buffer);
    free(avg_power_spec);
    snd_pcm_close (capture_handle);
    fclose(file_pointer);
    fftw_free(in_buffer_fft);
    fftw_free(out_buffer_fft);

    return 0;

}
