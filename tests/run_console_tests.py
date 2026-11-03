#!/usr/bin/env python3
"""Valida paridade CI/nativa, streams e restauração do terminal POSIX."""
import argparse
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default=str(ROOT / 'bin/ijkl'))
    parser.add_argument('--vm', default=str(ROOT / 'bin/ijkl-m'))
    options = parser.parse_args()
    passed = 0
    with tempfile.TemporaryDirectory(prefix='ijklang-console-') as directory:
        outputs = {}
        for native in (False, True):
            mode = 'native' if native else 'ci'
            programs = {}
            for fixture in ('probe', 'mixed', 'aliases', 'poll'):
                target = Path(directory) / f'{fixture}-{mode}.ci'
                language = 'pt' if fixture == 'aliases' else 'en'
                command = [options.compiler, '-ini', str(ROOT / f'tests/ijkl_{language}.ini')]
                if native:
                    command.append('--native-builtins')
                command += [str(ROOT / f'tests/console/{fixture}.ijk'), '-o', str(target)]
                result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=15)
                assert result.returncode == 0, result.stdout + result.stderr
                emitted = target.read_text()
                for builtin in ('has_key', 'read_key_async'):
                    assert (f'call #__builtin_console_{builtin};' in emitted) == native
                if not native:
                    assert 'in 0x0005, r0;' in emitted and 'in 0x0006, r0;' in emitted
                programs[fixture] = target
            cases = [
                ('empty', 'probe', b'', b'0\n0\n0\n0\n0\n0\n0\n'),
                ('buffered', 'probe', b'AB', b'1\n1\n65\n1\n66\n0\n0\n'),
                ('escape', 'probe', b'\x1b', b'1\n1\n27\n0\n0\n0\n0\n'),
                ('nul', 'probe', b'\x00', b'1\n1\n0\n0\n0\n0\n0\n'),
                ('mixed', 'mixed', b'hello\nZ', b'1\nhello\n1\n90\n'),
                ('aliases', 'aliases', b'Z', b'1\n90\n'),
            ]
            for name, fixture, data, expected in cases:
                result = subprocess.run([options.vm, str(programs[fixture])], input=data,
                                        capture_output=True, timeout=5, cwd=ROOT)
                actual = result.stdout.replace(b'\r\n', b'\n')
                assert result.returncode == 0 and not result.stderr, result.stderr
                assert actual == expected, (mode, name, actual, expected)
                outputs[(mode, name)] = actual
                passed += 1
                print(f'PASS {mode}: {name}')
            # O produtor permanece aberto: EOF não pode mascarar uma leitura bloqueante.
            with subprocess.Popen([options.vm, str(programs['probe'])], cwd=ROOT,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE) as process:
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    raise AssertionError('Leitura bloqueou em pipe aberto sem dados')
                assert process.returncode == 0
                assert process.stdout.read() == cases[0][3]
                assert not process.stderr.read()
                passed += 1
                print(f'PASS {mode}: open-empty-pipe')
            with tempfile.TemporaryFile() as input_file:
                input_file.write(b'AB')
                input_file.seek(0)
                result = subprocess.run([options.vm, str(programs['probe'])], cwd=ROOT,
                                        stdin=input_file, capture_output=True, timeout=5)
                assert result.returncode == 0 and result.stdout == cases[1][3]
                passed += 1
                print(f'PASS {mode}: regular-file')
            if os.name == 'posix':
                import pty
                import termios
                master, slave = pty.openpty()
                original = termios.tcgetattr(slave)
                process = subprocess.Popen([options.vm, str(programs['poll'])], cwd=ROOT,
                                           stdin=slave, stdout=slave, stderr=slave)
                transcript = b''
                try:
                    deadline = time.monotonic() + 5
                    while b'READY' not in transcript and time.monotonic() < deadline:
                        if select.select([master], [], [], 0.1)[0]:
                            transcript += os.read(master, 4096)
                    assert b'READY' in transcript, transcript
                    # ESC é enviado sem Enter, após a VM iniciar a sondagem.
                    os.write(master, b'\x1b')
                    process.wait(timeout=5)
                    while select.select([master], [], [], 0.1)[0]:
                        transcript += os.read(master, 4096)
                    assert process.returncode == 0 and b'27\r\n' in transcript, transcript
                    assert b'\x1b' not in transcript, transcript
                    assert termios.tcgetattr(slave) == original, 'Terminal não restaurado'
                    passed += 1
                    print(f'PASS {mode}: terminal-ESC-without-Enter-and-restoration')
                    # Um encerramento assíncrono também deve preservar o terminal.
                    import signal
                    interrupted = subprocess.Popen([options.vm, str(programs['poll'])], cwd=ROOT,
                                                   stdin=slave, stdout=slave, stderr=slave)
                    try:
                        ready = b''
                        deadline = time.monotonic() + 5
                        while b'READY' not in ready and time.monotonic() < deadline:
                            if select.select([master], [], [], 0.1)[0]:
                                ready += os.read(master, 4096)
                        assert b'READY' in ready, ready
                        interrupted.send_signal(signal.SIGTERM)
                        interrupted.wait(timeout=5)
                        assert interrupted.returncode == -signal.SIGTERM
                        assert termios.tcgetattr(slave) == original
                        passed += 1
                        print(f'PASS {mode}: terminal-restoration-on-SIGTERM')
                    finally:
                        if interrupted.poll() is None:
                            interrupted.kill()
                            interrupted.wait()
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
                    os.close(master)
                    os.close(slave)
        for name, *_ in cases:
            assert outputs[('ci', name)] == outputs[('native', name)]
    print(f'Console: {passed} passed; CI/native parity verified')


if __name__ == '__main__':
    main()
