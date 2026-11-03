#!/usr/bin/env python3
"""Verifica vetores independentes, bits IEEE 754, estado misto e sementes de relógio."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
MASK = (1 << 64) - 1


def oracle_next(seed):
    return ((seed * 6364136223846793005 + 1442695040888963407) & MASK) >> 33


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default=str(ROOT / 'bin/ijkl'))
    parser.add_argument('--vm', default=str(ROOT / 'bin/ijkl-m'))
    options = parser.parse_args()
    expected = (ROOT / 'tests/random/vectors.expected').read_bytes()
    count = 0
    with tempfile.TemporaryDirectory(prefix='ijklang-random-') as directory:
        programs = {}
        for native in (False, True):
            mode = 'native' if native else 'ci'
            for fixture in ('vectors', 'aliases', 'time_seed'):
                target = Path(directory) / f'{fixture}-{mode}.ci'
                language = 'pt' if fixture == 'aliases' else 'en'
                command = [options.compiler, '-ini', str(ROOT / f'tests/ijkl_{language}.ini')]
                if native:
                    command.append('--native-builtins')
                command += [str(ROOT / f'tests/random/{fixture}.ijk'), '-o', str(target)]
                result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=30)
                assert result.returncode == 0, result.stdout + result.stderr
                emitted = target.read_text()
                for name in ('seed', 'seed_with_time', 'next', 'next_range', 'next_real'):
                    assert (f'call #__builtin_random_{name};' in emitted) == native
                programs[(mode, fixture)] = target
            for repetition in range(2):
                result = subprocess.run([options.vm, str(programs[(mode, 'vectors')])],
                                        cwd=ROOT, capture_output=True, timeout=30)
                assert result.returncode == 0 and not result.stderr, result.stderr
                actual = result.stdout.replace(b'\r\n', b'\n')
                assert actual == expected, (mode, 'vectors mismatch')
                samples = list(map(int, actual.splitlines()))
                assert len(samples) == 460
                assert len(set(samples[2:385:3])) >= 50, 'Dispersão básica insuficiente'
                count += 1
                print(f'PASS {mode}: 460 oracle values (128 triples + boundary seeds/ranges), run {repetition + 1}')
            result = subprocess.run([options.vm, str(programs[(mode, 'aliases')])],
                                    cwd=ROOT, capture_output=True, timeout=15)
            assert result.returncode == 0 and not result.stderr, result.stderr
            assert result.stdout.replace(b'\r\n', b'\n') == b'2117897364\n3\n0.295513\n'
            count += 1
            print(f'PASS {mode}: Portuguese aliases')
            for timezone in ('UTC', 'America/New_York'):
                previous_seeds = set()
                for repetition in range(2):
                    if repetition:
                        time.sleep(1.2)
                    environment = dict(os.environ, TZ=timezone)
                    result = subprocess.run([options.vm, str(programs[(mode, 'time_seed')])],
                                            cwd=ROOT, capture_output=True, timeout=15, env=environment)
                    assert result.returncode == 0 and not result.stderr, result.stderr
                    before, after, generated = map(int, result.stdout.splitlines())
                    assert 0 <= before < 86400 and 0 <= after < 86400
                    window = (after - before) % 86400
                    assert window <= 5, 'Relógio variou inesperadamente durante o teste'
                    seeds = {(before + offset) % 86400 for offset in range(window + 1)}
                    assert generated in {oracle_next(seed) for seed in seeds}, (mode, timezone, result.stdout)
                    if repetition:
                        assert not previous_seeds.intersection(seeds)
                    previous_seeds = seeds
                    count += 1
                    print(f'PASS {mode}: seedWithTime {timezone}, run {repetition + 1}')
        # Faz next() executar em CI, enquanto seed/range/real permanecem nativos.
        expression = r'\n#ijkL\.Math\.Random\.next\$void:.*?(?=\n#ijkL\.Math\.Random\.nextReal\$void:)'
        ci_text = programs[('ci', 'vectors')].read_text()
        native_text = programs[('native', 'vectors')].read_text()
        section = re.search(expression, ci_text, re.S)
        assert section is not None
        mixed_text, replacements = re.subn(expression, lambda _: section.group(), native_text, flags=re.S)
        assert replacements == 1
        mixed = Path(directory) / 'mixed.ci'
        mixed.write_text(mixed_text)
        result = subprocess.run([options.vm, str(mixed)], cwd=ROOT, capture_output=True, timeout=30)
        assert result.returncode == 0 and not result.stderr, result.stderr
        assert result.stdout.replace(b'\r\n', b'\n') == expected
        count += 1
        print('PASS mixed CI/native: shared state, 460 oracle values')
    print(f'Random: {count} checks passed; exact integer/IEEE 754 parity verified')


if __name__ == '__main__':
    main()
