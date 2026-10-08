#!/usr/bin/env python3
"""Estimate the most likely musical keys of audio files.

Method: decode with ffmpeg -> STFT -> tuning-corrected chromagram ->
correlate against Krumhansl-Kessler and Temperley key profiles for all
24 keys -> softmax into rough probabilities.

Usage:
    python whatkey.py                  # analyze everything in ./audio-to-analyze
    python whatkey.py song.mp3 -n 8    # specific files, show top 8
    python whatkey.py song.wav --json  # machine-readable output
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

SR = 22050
N_FFT = 8192
HOP = 4096
NOTES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
AUDIO_EXTS = {".wav", ".mp3", ".flac", ".aiff", ".aif", ".m4a", ".ogg", ".opus", ".aac"}
DEFAULT_DIR = Path(__file__).parent / "audio-to-analyze"

# Major / minor profiles, index 0 = tonic.
PROFILES = {
    "krumhansl": (
        [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88],
        [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17],
    ),
    "temperley": (
        [5.0, 2.0, 3.5, 2.0, 4.5, 4.0, 2.0, 4.5, 2.0, 3.5, 1.5, 4.0],
        [5.0, 2.0, 3.5, 4.5, 2.0, 4.0, 2.0, 4.5, 3.5, 2.0, 1.5, 4.0],
    ),
}

# Camelot wheel numbers (DJ notation) indexed by tonic pitch class.
CAMELOT_MAJOR = [8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1]
CAMELOT_MINOR = [5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10]


def load_audio(path):
    cmd = ["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32)


def estimate_tuning(mag, freqs):
    """Global tuning offset in semitones, from interpolated spectral peaks in 200-2000 Hz
    (lower bins are too coarse to resolve cents at this FFT size)."""
    band = (freqs >= 200) & (freqs <= 2000)
    m, f0, df = mag[:, band], freqs[band][0], freqs[1] - freqs[0]
    a, b, c = m[:, :-2], m[:, 1:-1], m[:, 2:]
    peak = (b > a) & (b > c) & (b > 0.1 * m.max(axis=1, keepdims=True))
    fi, bi = np.nonzero(peak)
    a, b, c = (np.log(x[fi, bi] + 1e-12) for x in (a, b, c))
    offset = 0.5 * (a - c) / (a - 2 * b + c)  # parabolic interpolation
    pf = f0 + (bi + 1 + offset) * df
    dev = 69 + 12 * np.log2(pf / 440.0)
    angle = np.angle(np.sum(m[fi, bi + 1] * np.exp(2j * np.pi * dev)))
    return angle / (2 * np.pi)


def chroma_frames(y):
    """Return (frames x 12) chroma and per-frame energy."""
    if len(y) < N_FFT:
        y = np.pad(y, (0, N_FFT - len(y)))
    n_frames = 1 + (len(y) - N_FFT) // HOP
    idx = np.arange(N_FFT)[None, :] + HOP * np.arange(n_frames)[:, None]
    mag = np.abs(np.fft.rfft(y[idx] * np.hanning(N_FFT), axis=1))

    freqs = np.fft.rfftfreq(N_FFT, 1 / SR)
    band = (freqs >= 55) & (freqs <= 4000)  # A1 .. ~B7
    mag, freqs = mag[:, band], freqs[band]
    midi = 69 + 12 * np.log2(freqs / 440.0)

    tuning = estimate_tuning(mag, freqs)

    pc = np.round(midi - tuning).astype(int) % 12
    logmag = np.log1p(100 * mag / (mag.max() + 1e-12))
    chroma = np.stack([logmag[:, pc == k].sum(axis=1) for k in range(12)], axis=1)
    energy = chroma.sum(axis=1)
    return chroma, energy, tuning * 100


def key_scores(chroma_vec):
    """Average Pearson correlation of a 12-d chroma vector with every key profile."""
    scores = np.zeros(24)  # 0-11 major, 12-23 minor
    for major, minor in PROFILES.values():
        for mode, prof in enumerate((major, minor)):
            for tonic in range(12):
                scores[mode * 12 + tonic] += np.corrcoef(chroma_vec, np.roll(prof, tonic))[0, 1]
    return scores / len(PROFILES)


def key_name(i):
    tonic, minor = i % 12, i >= 12
    camelot = f"{(CAMELOT_MINOR if minor else CAMELOT_MAJOR)[tonic]}{'A' if minor else 'B'}"
    return f"{NOTES[tonic]} {'minor' if minor else 'major'}", camelot


def neighbors(i):
    """Harmonically closest keys: relative, dominant, subdominant."""
    tonic, minor = i % 12, i >= 12
    base = 12 if minor else 0
    relative = (tonic + 3) % 12 if minor else 12 + (tonic + 9) % 12
    return {
        "relative": relative,
        "dominant (V)": base + (tonic + 7) % 12,
        "subdominant (IV)": base + (tonic + 5) % 12,
    }


def analyze(path, top_n, temperature):
    """Return a dict describing the likely keys of one audio file (None if empty)."""
    y = load_audio(path)
    if not len(y):
        return None
    chroma, energy, cents = chroma_frames(y)
    keep = energy > 0.1 * np.median(energy)  # ignore near-silent frames
    chroma = chroma / (energy[:, None] + 1e-12)
    profile = chroma[keep].mean(axis=0)

    scores = key_scores(profile)
    probs = np.exp(scores / temperature)
    probs /= probs.sum()
    order = np.argsort(-probs)

    # Stability: how often segments (~30s, or ~1/4 of short clips) agree with the winner.
    seg_sec = min(30, max(5, len(y) / SR / 4))
    seg = max(1, int(seg_sec * SR / HOP))
    seg_winners = [
        np.argmax(key_scores(chroma[s:s + seg][keep[s:s + seg]].mean(axis=0)))
        for s in range(0, len(chroma), seg) if keep[s:s + seg].sum() > seg // 4
    ]
    agree = float(np.mean([w == order[0] for w in seg_winners])) if seg_winners else None

    def entry(i, **extra):
        name, cam = key_name(i)
        return {"key": name, "camelot": cam, "prob": round(float(probs[i]), 4), **extra}

    return {
        "file": path.name,
        "duration": round(len(y) / SR, 1),
        "tuning_cents": round(float(cents), 1),
        "keys": [entry(i) for i in order[:top_n]],
        "segment_agreement": agree,
        "segments": len(seg_winners),
        "segment_seconds": round(seg_sec, 1),
        "compatible": [entry(j, relation=label) for label, j in neighbors(order[0]).items()],
    }


def print_report(r):
    print(f"\n{r['file']}  ({r['duration']:.0f}s, tuning {r['tuning_cents']:+.0f} cents)")
    print("-" * 52)
    for rank, k in enumerate(r["keys"], 1):
        bar = "#" * int(round(k["prob"] * 40))
        print(f"{rank:>2}. {k['key']:<10} {k['camelot']:>4}  {k['prob'] * 100:5.1f}%  {bar}")
    agree = "n/a" if r["segment_agreement"] is None else f"{r['segment_agreement'] * 100:.0f}%"
    print(f"Segment agreement with #1: {agree} of {r['segments']} x {r['segment_seconds']:.0f}s chunks")
    print("Harmonically compatible with #1:")
    for k in r["compatible"]:
        print(f"    {k['relation']:<17} {k['key']:<10} {k['camelot']:>4}  {k['prob'] * 100:5.1f}%")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", type=Path, help=f"files or folders (default: {DEFAULT_DIR.name}/)")
    ap.add_argument("-n", "--top", type=int, default=5, help="number of keys to show (default 5)")
    ap.add_argument("-t", "--temperature", type=float, default=0.05,
                    help="softmax temperature; lower = more confident-looking (default 0.05)")
    ap.add_argument("--json", action="store_true", help="print one JSON object per file instead of a report")
    args = ap.parse_args()

    files = []
    for p in args.paths or [DEFAULT_DIR]:
        if p.is_dir():
            files += sorted(f for f in p.iterdir() if f.suffix.lower() in AUDIO_EXTS)
        elif p.exists():
            files.append(p)
        else:
            print(f"not found: {p}", file=sys.stderr)
    if not files:
        sys.exit(f"No audio files found. Drop some into {DEFAULT_DIR}/")
    for f in files:
        try:
            r = analyze(f, args.top, args.temperature)
        except subprocess.CalledProcessError as e:
            print(f"{f.name}: ffmpeg failed: {e.stderr.decode().strip()}", file=sys.stderr)
            continue
        if r is None:
            print(f"{f.name}: no audio decoded, skipping", file=sys.stderr)
        elif args.json:
            print(json.dumps(r), flush=True)
        else:
            print_report(r)


if __name__ == "__main__":
    main()
