#! /bin/bash

DEVICE="hw:1,1"    # ALSA device (run `arecord -l` to list)
RATE=96000         # Sample rate (Hz)
DURATION=10        # Duration in seconds

DATETIME=$(date +'%Y%m%d_%H%M%S')
FILENAME="${DATETIME}_${RATE}Hz_${DURATION}s.wav"

echo "Recording ${DURATION}s from ${DEVICE} at ${RATE}Hz..."
echo "Saving to: ${FILENAME}"

arecord -D "$DEVICE" -c 2 -d "$DURATION" -r "$RATE" -f S32_LE "$FILENAME"