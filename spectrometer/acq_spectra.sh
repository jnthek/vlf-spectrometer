#!/bin/bash

# The following command finds the USB soundcard, and stores in Device ID in a variable that can be fed to the spectrometer code.
AUDIO_ID=$(arecord -l | grep -i "USB" | head -n 1 | sed -E 's/card ([0-9]+):.*device ([0-9]+):.*/hw:\1,\2/')

# Add a guard rail to not try alsaspectrometer if there is no suitable soundcard
if [ -z "$AUDIO_ID" ]; then
    echo "Error: the specified audio capture device is not detected!" >&2
else
    echo "Successfully bound to the specified hardware target: $AUDIO_ID"
    echo "Using device ${AUDIO_ID}"
    # ./alsaspectrometer -D ${AUDIO_ID} -N 32768 -r 96000 -i 1 -T 10
    ./alsaspectrometer -D ${AUDIO_ID} -N 32768 -r 48000 -i 1 -T 10 -C 1
fi

#arecord -D ${AUDIO_ID} -c 2 -d 60 -r 96000 -f S32_LE test_audio.wav



