R"MAMBARAW(
from compileall import compile_file
from concurrent.futures import ProcessPoolExecutor
import sys

def main():
    results = []
    with sys.stdin:
        with ProcessPoolExecutor(max_workers=None) as executor:
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
