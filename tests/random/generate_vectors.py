"""Gera vetores de referência usando inteiros arbitrários do Python, sem executar a VM."""
from pathlib import Path
import struct

DIRECTORY = Path(__file__).resolve().parent
MASK = (1 << 64) - 1
state = 1

def advance():
    global state
    state = (state * 6364136223846793005 + 1442695040888963407) & MASK
    return state


def next_integer():
    return advance() >> 33


def next_real_bits():
    value = (advance() >> 11) / (1 << 53)
    return struct.unpack('<Q', struct.pack('<d', value))[0]


def next_range(minimum, maximum):
    minimum, maximum = sorted((minimum, maximum))
    if minimum == maximum:
        return minimum
    distance = maximum - minimum
    shift = 64 - distance.bit_length()
    while True:
        candidate = advance() >> shift
        if candidate <= distance:
            return minimum + candidate


def literal(value):
    return '(-9223372036854775807 - 1)' if value == -(1 << 63) else str(value)


def generate():
    global state
    state = 1
    expected = [next_integer()]
    lines = ['''// Vetores de oráculo independente: python3 tests/random/generate_vectors.py.
import ijkL.Math.Random;
program RandomVectors {
    // A porta numérica imprime os bits IEEE 754, sem arredondar a seis casas.
    private printRealBits(real value) {
        asm {
            r1 := [ra, 0];
            out 0x0003, r1;
            out 0x0000, '\\n';
        }
    }
    private printInteger(int value) {
        asm {
            r1 := [ra, 0];
            out 0x0003, r1;
            out 0x0000, '\\n';
        }
    }
    private main(): int {
        printInteger(ijkL.Math.Random.next());
        ijkL.Math.Random.seed(123456789);
        int index = 0;
        while (index < 128) {
            printInteger(ijkL.Math.Random.next());
            printInteger(ijkL.Math.Random.nextRange(1, 100));
            printRealBits(ijkL.Math.Random.nextReal());
            index = index + 1;
        }
''']
    state = 123456789
    for _ in range(128):
        expected += [next_integer(), next_range(1, 100), next_real_bits()]
    bounds = [(0, 0), (-7, -7), (5, -5), (0, (1 << 31) - 1), (0, 1 << 32),
              (-(1 << 63), (1 << 63) - 1), (-(1 << 63), -(1 << 63) + 1),
              ((1 << 63) - 2, (1 << 63) - 1), (-(1 << 32), 1 << 32),
              (-1, (1 << 63) - 1), (-(1 << 63), 0), (0, (1 << 63) - 1),
              (-(1 << 63), -(1 << 63) + (1 << 32))]
    for initial_seed in (0, 1, -1, -(1 << 63), (1 << 63) - 1):
        state = initial_seed & MASK
        lines.append(f'        ijkL.Math.Random.seed({literal(initial_seed)});\n')
        for lower, upper in bounds:
            lines.append(f'        printInteger(ijkL.Math.Random.nextRange({literal(lower)}, {literal(upper)}));\n')
            expected.append(next_range(lower, upper))
        lines.append('        printInteger(ijkL.Math.Random.next());\n        printRealBits(ijkL.Math.Random.nextReal());\n')
        expected += [next_integer(), next_real_bits()]
    lines.append('        return 0;\n    }\n}\n')
    (DIRECTORY / 'vectors.ijk').write_text(''.join(lines))
    (DIRECTORY / 'vectors.expected').write_text(''.join(f'{value}\n' for value in expected))


if __name__ == '__main__':
    generate()
