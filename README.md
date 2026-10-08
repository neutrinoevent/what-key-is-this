# what-key-is-this

A small CLI that estimates the most likely musical keys of audio files, with
Camelot codes and harmonically compatible keys.

## Requirements

- Python 3.9+
- [numpy](https://numpy.org/) (`pip install -r requirements.txt`)
- [ffmpeg](https://ffmpeg.org/) on your `PATH` (used to decode wav, mp3, flac, m4a, ogg, aiff, ...)

## Usage

Drop audio files into `audio-to-analyze/` and run:

```
python3 whatkey.py                  # analyzes everything in audio-to-analyze/
python3 whatkey.py song.mp3 -n 8    # specific files/folders, show top 8 keys
python3 whatkey.py -t 0.1           # softer (less peaky) percentages
python3 whatkey.py song.wav --json  # machine-readable output
```

Example output:

```
my-track.wav  (24s, tuning +1 cents)
----------------------------------------------------
 1. E major     12B   42.4%  #################
 2. F# minor    11A   33.3%  #############
 3. A major     11B   21.5%  #########
 4. A minor      8A    0.9%
 5. Ab minor     1A    0.6%
Segment agreement with #1: 75% of 4 x 6s chunks
Harmonically compatible with #1:
    relative          C# minor    12A    0.0%
    dominant (V)      B major      1B    0.6%
    subdominant (IV)  A major     11B   21.5%
```

- **Camelot code**: DJ wheel notation; adjacent numbers / same number with A↔B mix well.
- **Percentage**: relative confidence (softmax of profile correlations), not a calibrated probability.
- **Segment agreement**: how often shorter chunks of the track pick the same top key; low values suggest modulation or ambiguity.
- Keys that share most of their notes (e.g. E major / A major / F# minor) often land close together; that's expected.

## Ableton Live device (Max for Live)

Requires Live Suite (or Max for Live). From the repo folder:

```
python3 ableton/build_device.py
```

This installs **WhatKey** into `User Library/Presets/Audio Effects/Max Audio Effect/`.
Drag it onto any audio track (audio passes through untouched), then drop an audio file,
from Finder or Live's browser, onto the left panel. The key results appear in the device.

The device points at this folder by absolute path and runs `whatkey.py` with the Python that ran
the build script, so re-run the build if you move the repo or switch Python installs.

## How it works

1. Decode to mono 22.05 kHz with ffmpeg.
2. STFT, then estimate global tuning offset from interpolated spectral peaks (handles tracks not at A=440).
3. Fold log-compressed spectral energy (55 Hz–4 kHz) into a 12-bin chromagram, skipping near-silent frames.
4. Correlate the average chroma with Krumhansl-Kessler and Temperley major/minor profiles for all 24 keys and average the two.
5. Softmax the scores into percentages.

## License

[MIT](LICENSE) © neutrinoevent
