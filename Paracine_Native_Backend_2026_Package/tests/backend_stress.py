#!/usr/bin/env python3
"""Deterministic native/PCIR differential stress test for Paracine's x64 backend."""
import argparse
from pathlib import Path
import random
import subprocess
import tempfile


def run(compiler, workdir):
    randomizer = random.Random(20261110)
    count = 0
    for n in range(24):
        multiplier = randomizer.choice([1, 3, 7, 15, 31, 127, 299])
        add = randomizer.choice([-5000, -300, -127, -1, 0, 1, 12, 127, 255, 30000, 2147483648])
        xor = randomizer.choice([0, 1, 31, 63, 511, 2047])
        shift = randomizer.choice([0, 1, 2, 7, 31, 63, 64, 65, 129])
        limit = randomizer.choice([-32768, -300, -1, 0, 1, 33, 1234, 30000])
        seed = randomizer.randint(-300, 300)
        code = (
            f"module stress{n}\n"
            "routine work(x: int) gives int\n"
            "    var v: int = x\n"
            f"    v = (v * {multiplier} + ({add})) ^ {xor}\n"
            f"    if v > ({limit})\n"
            f"        v = v >> {shift}\n"
            "    else\n"
            f"        v = v << {shift}\n"
            "    give v & 255\n"
            "routine main() gives int\n"
            f"    give work({seed})\n"
        )
        source = workdir / f"stress_{n:02d}.pcn"
        source.write_text(code, encoding="utf-8")
        for level in (0, 2, 3):
            proc = subprocess.run([str(compiler), "test", str(source), f"-O{level}"],
                                  capture_output=True, text=True, timeout=60)
            if proc.returncode != 0 or "test passed" not in proc.stdout:
                raise RuntimeError(f"Case {n} -O{level} failed:\n{proc.stdout}\n{proc.stderr}\n{code}")
            count += 1
    print(f"PASS: {count} differential PCIR/native runs on 24 deterministic programs (-O0/-O2/-O3)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="paracine-backend-") as directory:
        run(args.compiler.resolve(), Path(directory))
