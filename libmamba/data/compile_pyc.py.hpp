R"MAMBARAW(
from compileall import compile_file
from concurrent.futures import ProcessPoolExecutor
import os
import sys

def main():
    max_workers = int(os.environ.get("MAMBA_EXTRACT_THREADS", "0"))
    if max_workers <= 0:
        max_workers = None

    results = []
    with sys.stdin:
        with ProcessPoolExecutor(max_workers=max_workers) as executor:
            while True:
                name = sys.stdin.readline().strip()
                if not name:
                    break
                results.append(executor.submit(compile_file, name, quiet=1))
            success = all(r.result() for r in results)
    return success

if __name__ == "__main__":
    success = main()
    sys.exit(int(not success))

)MAMBARAW"
