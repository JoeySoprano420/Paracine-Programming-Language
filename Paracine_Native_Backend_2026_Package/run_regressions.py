#!/usr/bin/env python3
"""External Paracine frontend/native regression runner (Python 3.9+)."""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
TEST_DIR = ROOT / "tests"


def command(compiler, *args, expected=0):
    proc = subprocess.run([str(compiler), *map(str, args)],
                          text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=90)
    if proc.returncode != expected:
        raise AssertionError(
            f"expected exit {expected}, got {proc.returncode} from {args}:\n"
            f"STDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")
    return proc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=pathlib.Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    successes = 0
    for name in ("alias_types", "precedence_shift", "unicode_escape", "trailing_comma"):
        proc = command(compiler, "test", TEST_DIR / (name + ".pcn"))
        if "test passed" not in proc.stdout:
            raise AssertionError(f"{name}: native/oracle test did not pass: {proc.stdout}")
        print(f"PASS native/oracle: {name}")
        successes += 1
    for name, expected_code in (("invalid_escape", "E0116"),
                                ("invalid_alias", "E0233")):
        result = command(compiler, "check", TEST_DIR / (name + ".pcn"),
                         "--json", expected=1)
        data = json.loads(result.stdout)
        assert not data["ok"]
        assert data["diagnostics"][0]["code"] == expected_code
        assert data["diagnostics"][0]["line"] > 0
        print(f"PASS diagnostic: {name} ({expected_code})")
        successes += 1
    command(compiler, "selftest")
    print(f"PASS built-in compiler selftest; external regressions: {successes} passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
