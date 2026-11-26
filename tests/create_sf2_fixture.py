"""Generate our own tiny looping sine bank, no third-party samples or presets."""
import math
import struct
from pathlib import Path


def chunk(name, data):
    return name.encode('ascii') + struct.pack('<I', len(data)) + data + b'\0' * (len(data) & 1)


def name(text):
    return text.encode('ascii')[:19].ljust(20, b'\0')


def create(path, multiple=False):
    rate, count = 44100, 44100
    samples = [round(12000 * math.sin(2 * math.pi * 440 * n / rate)) for n in range(count)] + [0] * 46
    info = chunk('LIST', b'INFO' + chunk('ifil', struct.pack('<HH', 2, 1))
                 + chunk('isng', b'EMU8000\0') + chunk('INAM', b'FengYin test sine\0'))
    sdta = chunk('LIST', b'sdta' + chunk('smpl', struct.pack('<%dh' % len(samples), *samples)))
    n = 2 if multiple else 1
    phdr = b''.join(name('Sine %d' % i) + struct.pack('<HHHIII', i, 0, i, 0, 0, 0) for i in range(n))
    phdr += name('EOP') + struct.pack('<HHHIII', 0, 0, n, 0, 0, 0)
    pbag = b''.join(struct.pack('<HH', i, 0) for i in range(n + 1))
    pgen = b''.join(struct.pack('<HH', 41, 0) for _ in range(n)) + struct.pack('<HH', 0, 0)
    inst = name('Sine instrument') + struct.pack('<H', 0) + name('EOI') + struct.pack('<H', 1)
    generators = [(34, -6000), (38, -3600), (54, 1), (58, 69), (53, 0)]
    igen = b''.join(struct.pack('<Hh', k, v) for k, v in generators) + b'\0' * 4
    ibag = struct.pack('<HHHH', 0, 0, len(generators), 0)
    shdr = name('Sine') + struct.pack('<IIIIIBbHH', 0, count, 0, count, rate, 69, 0, 0, 1)
    shdr += name('EOS') + b'\0' * 26
    pdta = chunk('LIST', b'pdta' + b''.join(chunk(k, v) for k, v in [
        ('phdr', phdr), ('pbag', pbag), ('pmod', b'\0'*10), ('pgen', pgen),
        ('inst', inst), ('ibag', ibag), ('imod', b'\0'*10), ('igen', igen), ('shdr', shdr)]))
    Path(path).write_bytes(chunk('RIFF', b'sfbk' + info + sdta + pdta))


if __name__ == '__main__':
    import sys
    create(sys.argv[1], '--multiple' in sys.argv[2:])
