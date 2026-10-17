#!/usr/bin/env python3
"""Small project helper for configure, build, and flash.

Run from anywhere:
    python tools/tool.py build
    python tools/tool.py flash
    python tools/tool.py all
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / ".build" / "debug"
TOOLCHAIN_FILE = ROOT / "cmake" / "gcc-arm-none-eabi.cmake"
HEX_FILE = BUILD_DIR / "SuperCap_V3_1.hex"

BUILD_TOOLS = (
    "cmake",
    "ninja",
    "arm-none-eabi-gcc",
    "arm-none-eabi-objcopy",
    "arm-none-eabi-size",
)
REQUIRED_TOOLS = (*BUILD_TOOLS, "STM32_Programmer_CLI")


def run(cmd: list[str]) -> None:
    print(">", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise SystemExit(f"Missing tool in PATH: {name}")
    return path


def require_tools(names: tuple[str, ...]) -> dict[str, str]:
    return {name: require_tool(name) for name in names}


def configure(_: argparse.Namespace) -> None:
    tools = require_tools(BUILD_TOOLS)
    run(
        [
            tools["cmake"],
            "-S",
            str(ROOT),
            "-B",
            str(BUILD_DIR),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN_FILE}",
            "-DCMAKE_BUILD_TYPE=Debug",
        ]
    )


def build(args: argparse.Namespace) -> None:
    if args.configure:
        configure(args)
    cmake = require_tool("cmake")
    run([cmake, "--build", str(BUILD_DIR)])


def flash(args: argparse.Namespace) -> None:
    programmer = require_tool("STM32_Programmer_CLI")
    if args.build:
        args.configure = True
        build(args)
    if not HEX_FILE.exists():
        raise SystemExit(f"Hex file not found: {HEX_FILE}. Run build first.")
    run(
        [
            programmer,
            "-c",
            "port=SWD",
            "mode=UR",
            "reset=HWrst",
            f"freq={args.freq}",
            "-w",
            str(HEX_FILE),
            "-v",
            "-rst",
        ]
    )


def all_steps(args: argparse.Namespace) -> None:
    args.configure = True
    args.build = False
    build(args)
    args.build = False
    flash(args)


def paths(_: argparse.Namespace) -> None:
    print(f"root: {ROOT}")
    print(f"build: {BUILD_DIR}")
    print(f"toolchain: {TOOLCHAIN_FILE}")
    print(f"hex: {HEX_FILE}")


def doctor(_: argparse.Namespace) -> int:
    """Report the interpreter and external tools without changing the project."""
    print(f"python executable: {sys.executable}")
    print(f"python version: {sys.version.split()[0]}")

    missing: list[str] = []
    for name in REQUIRED_TOOLS:
        path = shutil.which(name)
        if path is None:
            missing.append(name)
            print(f"{name}: MISSING")
        else:
            print(f"{name}: {path}")

    print("preflight: OK" if not missing else f"preflight: MISSING {', '.join(missing)}")
    return 0 if not missing else 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="SuperCap project helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_configure = sub.add_parser("configure", help="configure CMake")
    p_configure.set_defaults(func=configure)

    p_build = sub.add_parser("build", help="build firmware")
    p_build.add_argument("--no-configure", dest="configure", action="store_false")
    p_build.set_defaults(func=build, configure=True)

    p_flash = sub.add_parser("flash", help="flash existing or freshly built hex")
    p_flash.add_argument("--build", action="store_true", help="build before flashing")
    p_flash.add_argument("--freq", default="4000", help="SWD frequency in KHz")
    p_flash.set_defaults(func=flash)

    p_all = sub.add_parser("all", help="configure, build, and flash")
    p_all.add_argument("--freq", default="4000", help="SWD frequency in KHz")
    p_all.set_defaults(func=all_steps)

    p_paths = sub.add_parser("paths", help="print resolved project paths")
    p_paths.set_defaults(func=paths)

    p_doctor = sub.add_parser(
        "doctor", aliases=["preflight"], help="report Python and required tools"
    )
    p_doctor.set_defaults(func=doctor)

    args = parser.parse_args(argv)
    result = args.func(args)
    return result if isinstance(result, int) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
