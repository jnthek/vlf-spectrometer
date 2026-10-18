all: alsaspectrometer

alsaspectrometer: alsaspectrometer.c
	@echo "ALSA spectrometer"
	gcc -g -Wall alsaspectrometer.c -o alsaspectrometer -lfftw3 -lm -lrt -lpthread -lasound

clean : 
	rm alsaspectrometer