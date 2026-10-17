import sys

from tool import main


if __name__ == "__main__":
    raise SystemExit(main(["build", *sys.argv[1:]]))
