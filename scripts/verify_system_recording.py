"""Validate the helper's real WAV output; no codec or browser dependency."""
import array
import math
import sys
import wave

with wave.open(sys.argv[1], 'rb') as source:
    assert (source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getnframes()) == (2, 2, 48000, 96000)
    pcm = array.array('h', source.readframes(96000))
assert len(pcm) == 192000
for start in range(0, len(pcm), 1920):
    block = pcm[start:start+1920]
    assert 1800 < math.sqrt(sum(v*v for v in block)/len(block)) < 2400
assert all(pcm[i] == pcm[i+1] for i in range(0, len(pcm), 2))
print('System-output WAV: 48kHz stereo PCM, exact duration and continuous signal passed')
