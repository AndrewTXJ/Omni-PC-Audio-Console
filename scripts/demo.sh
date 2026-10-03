#!/usr/bin/env sh
# Render the roadmap's headline job through the engine, so it can be heard
# rather than asserted: game, music and mic to the headphones, but the music
# kept off the stream.
#
# Needs: a built omni-render, and python3 (only to synthesise the input files).
# Writes everything to ./demo-out/.
#
# Usage:  scripts/demo.sh [path/to/omni-render]
set -eu

RENDER="${1:-./build/offline/omni-render}"
OUT="demo-out"

if [ ! -x "$RENDER" ]; then
  echo "demo: $RENDER not found or not executable." >&2
  echo "demo: build first:" >&2
  echo "  cmake -S . -B build -G Ninja && cmake --build build" >&2
  exit 1
fi

mkdir -p "$OUT"

echo "1/3  synthesising three sources into $OUT/ ..."
python3 - "$OUT" <<'PY'
import math, struct, sys, wave

d = sys.argv[1]
RATE = 48000
SECONDS = 6.0
N = int(RATE * SECONDS)

def save(name, gen):
    with wave.open(f"{d}/{name}", "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        frames = bytearray()
        for i in range(N):
            l, r = gen(i)
            for v in (l, r):
                s = int(round(max(-1.0, min(1.0, v)) * 32767))
                frames += struct.pack("<h", s)
        w.writeframes(bytes(frames))
    print(f"       {name}")

def env(t, period, attack=0.005, decay=0.25):
    """A simple percussive envelope, repeating every `period` seconds."""
    p = t % period
    if p < attack:
        return p / attack
    return math.exp(-(p - attack) / decay)

# GAME: a rhythmic low pulse with a blip, two beats a second.
def game(i):
    t = i / RATE
    e = env(t, 0.5)
    low = 0.45 * math.sin(2 * math.pi * 110 * t) * e
    blip = 0.18 * math.sin(2 * math.pi * 880 * t) * env(t, 0.5, 0.002, 0.03)
    v = (low + blip) * 0.6
    return v, v

# MUSIC: an A-major arpeggio, clearly melodic so its absence is obvious.
NOTES = [440.0, 554.37, 659.25, 880.0, 659.25, 554.37]
def music(i):
    t = i / RATE
    step = int(t / 0.25) % len(NOTES)
    f = NOTES[step]
    e = env(t, 0.25, 0.004, 0.12)
    v = 0.30 * math.sin(2 * math.pi * f * t) * e
    # Slight stereo spread so it is distinguishable from the mono sources.
    return v, 0.85 * v

# MIC: amplitude-modulated low tone, speech-like cadence.
def mic(i):
    t = i / RATE
    syllable = 0.5 * (1.0 + math.sin(2 * math.pi * 3.2 * t))
    gate = 1.0 if (t % 2.0) < 1.2 else 0.0
    carrier = math.sin(2 * math.pi * 196 * t) + 0.4 * math.sin(2 * math.pi * 392 * t)
    v = 0.33 * carrier * syllable * gate
    return v, v

save("game.wav", game)
save("music.wav", music)
save("mic.wav", mic)
PY

echo "2/3  routing through the engine ..."
# Strip 0 game, 1 music, 2 mic.
# Bus 0 = A1 headphones: everything.
# Bus 1 = B1 stream:     game + mic, music NOT ticked -- that is all mix-minus is.
# Faders at -7 dB: three sources summing into one bus otherwise reach full scale
# and engage the safety limiter, which is the limiter doing its job and a bad
# demo. The roadmap's gain-staging advice is peaks around -12 to -6 dBFS.
"$RENDER" \
  --strip "$OUT/game.wav"  --fader -7 --route 0:0 --route 0:1 \
  --strip "$OUT/music.wav" --fader -7 --route 1:0 \
  --strip "$OUT/mic.wav"   --fader -7 --route 2:0 --route 2:1 \
  --bus "0:$OUT/A1-headphones.wav:s24" \
  --bus "1:$OUT/B1-stream.wav:s24" \
  --verbose

echo "3/3  verifying, so this does not rest on trusting your ears ..."
python3 - "$OUT" <<'VERIFY'
import cmath, math, sys

d = sys.argv[1]

def read_s24_left(path):
    with open(path, 'rb') as f:
        b = f.read()
    i = b.find(b'data')
    body = b[i + 8:]
    out = []
    for k in range(0, len(body) - 2, 3):
        v = body[k] | (body[k + 1] << 8) | (body[k + 2] << 16)
        if v & 0x800000:
            v -= 0x1000000
        out.append(v / 8388608.0)
    return out[0::2]

def level_db(x, hz, rate=48000, n=48000):
    n = min(n, len(x))
    acc = sum(x[i] * cmath.exp(-2j * math.pi * hz * i / rate) for i in range(n))
    return 20 * math.log10(max(abs(acc) * 2 / n, 1e-12))

a1 = read_s24_left(f'{d}/A1-headphones.wav')
b1 = read_s24_left(f'{d}/B1-stream.wav')

# 659.25 Hz is in the arpeggio and in neither of the other two sources.
MUSIC_HZ, GAME_HZ = 659.25, 110.0
a1_music, b1_music = level_db(a1, MUSIC_HZ), level_db(b1, MUSIC_HZ)
a1_game, b1_game = level_db(a1, GAME_HZ), level_db(b1, GAME_HZ)
peak_a1 = max(abs(v) for v in a1)
peak_b1 = max(abs(v) for v in b1)

print(f'       music ({MUSIC_HZ:.0f} Hz):  A1 {a1_music:7.1f} dBFS   B1 {b1_music:7.1f} dBFS')
print(f'       game  ({GAME_HZ:.0f} Hz):   A1 {a1_game:7.1f} dBFS   B1 {b1_game:7.1f} dBFS')
print(f'       peak:              A1 {20*math.log10(peak_a1):7.1f} dBFS   B1 {20*math.log10(peak_b1):7.1f} dBFS')

ok = True
if a1_music <= b1_music + 30:
    print(f'       FAIL: music only {a1_music - b1_music:.1f} dB down on B1')
    ok = False
if b1_game <= b1_music + 20:
    print('       FAIL: the game should dominate music on the stream bus')
    ok = False
if peak_a1 >= 0.999 or peak_b1 >= 0.999:
    print('       FAIL: a bus reached full scale; gain staging is wrong')
    ok = False
if not ok:
    raise SystemExit(1)
print(f'       OK: music is {a1_music - b1_music:.0f} dB down on the stream bus, and neither bus clipped.')
VERIFY

echo "3/3  done."
echo
echo "Listen to these two files:"
echo "  $OUT/A1-headphones.wav   pulse + arpeggio + voice   (everything)"
echo "  $OUT/B1-stream.wav       pulse + voice, NO arpeggio (music excluded)"
echo
echo "The only difference is one unticked send: strip 1 (music) is not routed to"
echo "bus 1. That is mix-minus, and it is the same mechanism that keeps a call"
echo "app from hearing itself."
echo
echo "Play them with whatever you have, e.g.:  aplay $OUT/A1-headphones.wav"
