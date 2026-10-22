all: alsaspectrometer

CFLAGS += -Wall -O2 -g

#Pass the observatory ident here
# CFLAGS += -DOBS=\"VK6JN\"

#Pass the AUDFMT as SND_PCM_FORMAT_S16_LE if using an external 16 bit soundcard. DO NOT USE THIS FOR PCM1808 !
CFLAGS += -DAUDFMT=SND_PCM_FORMAT_S16_LE

#If this is defined, float64 aka double will be used as the format for data. If not, float32 will be used.
# CFLAGS += -DUSEDOUBLE

alsaspectrometer: alsaspectrometer.c
	@echo "Compiling ALSA spectrometer"
	gcc $(CFLAGS) alsaspectrometer.c -o alsaspectrometer -lfftw3 -lm -lrt -lpthread -lasound

clean : 
	rm alsaspectrometer