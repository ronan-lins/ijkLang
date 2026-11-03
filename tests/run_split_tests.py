#!/usr/bin/env python3
"""
Validador de paridade do utilitario de divisao de strings (ijkL.Text.split e splitLines).
Compara o modo padrao (CI) com o modo nativo (--native-builtins).
"""
import argparse
import os
import subprocess
import sys
import tempfile

def run_cmd(args, cwd=None):
    proc = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return proc.returncode, proc.stdout, proc.stderr

def main():
    parser = argparse.ArgumentParser(description="Parity tests for split and splitLines")
    parser.add_argument("--compiler", default="bin/Debug/ijkl")
    parser.add_argument("--vm", default="bin/Debug/ijkl-m")
    args = parser.parse_args()

    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    compiler = os.path.abspath(os.path.join(project_root, args.compiler)) if not os.path.isabs(args.compiler) else args.compiler
    vm = os.path.abspath(os.path.join(project_root, args.vm)) if not os.path.isabs(args.vm) else args.vm
    if not os.path.exists(compiler) and os.path.exists(compiler + ".exe"):
        compiler = compiler + ".exe"
    if not os.path.exists(vm) and os.path.exists(vm + ".exe"):
        vm = vm + ".exe"
    test_source = os.path.join(project_root, "tests", "text", "split_tests.ijk")
    ini_path = os.path.join(project_root, "tests", "ijkl_en.ini")

    with tempfile.TemporaryDirectory() as tmpdir:
        ci_target = os.path.join(tmpdir, "split_tests_ci.ci")
        native_target = os.path.join(tmpdir, "split_tests_native.ci")

        # 1. Compile CI
        ret, stdout, stderr = run_cmd([compiler, "-ini", ini_path, test_source, "-o", ci_target], cwd=project_root)
        if ret != 0:
            print(f"[FAIL] Falha ao compilar CI: {stderr}\n{stdout}", file=sys.stderr)
            return 1

        # 2. Compile Native
        ret, stdout, stderr = run_cmd([compiler, "-ini", ini_path, "--native-builtins", test_source, "-o", native_target], cwd=project_root)
        if ret != 0:
            print(f"[FAIL] Falha ao compilar Native: {stderr}\n{stdout}", file=sys.stderr)
            return 1

        # 3. Run CI
        ret_ci, out_ci, err_ci = run_cmd([vm, ci_target], cwd=project_root)
        if ret_ci != 0 or "ALL_SPLIT_TESTS_PASSED" not in out_ci:
            print(f"[FAIL] Execucao CI falhou: code={ret_ci}, err={err_ci}\n{out_ci}", file=sys.stderr)
            return 1
        print("PASS ci: All split and splitLines tests passed")

        # 4. Run Native
        ret_nat, out_nat, err_nat = run_cmd([vm, native_target], cwd=project_root)
        if ret_nat != 0 or "ALL_SPLIT_TESTS_PASSED" not in out_nat:
            print(f"[FAIL] Execucao Native falhou: code={ret_nat}, err={err_nat}\n{out_nat}", file=sys.stderr)
            return 1
        print("PASS native: All split and splitLines tests passed")

        # 5. Parity
        clean_ci = out_ci.strip().replace("\r\n", "\n")
        clean_nat = out_nat.strip().replace("\r\n", "\n")
        if clean_ci != clean_nat:
            print(f"[FAIL] Paridade violada! CI: '{clean_ci}' vs Native: '{clean_nat}'", file=sys.stderr)
            return 1

        print("PASS parity: CI and Native outputs are 100% IDENTICAL")
        print("Text Split: All checks passed; exact parity verified")
        return 0

if __name__ == "__main__":
    sys.exit(main())
