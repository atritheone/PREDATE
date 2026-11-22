"""Compatibility entry point for the Electron launcher."""
import argparse
import subprocess
from app_runtime import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--continue", dest="resume", action="store_true")
    parser.add_argument("--save-quit", action="store_true")
    args = parser.parse_args()
    executable = ROOT / "PREDATE.exe"
    if not executable.is_file():
        raise RuntimeError("Build the desktop launcher with npm run package in launcher.")
    command = [str(executable)]
    if args.resume: command.append("--continue")
    if args.save_quit: command.append("--save-quit")
    subprocess.Popen(command, cwd=ROOT)


if __name__ == "__main__": main()
