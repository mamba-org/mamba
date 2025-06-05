import ast
import sys
import pathlib
import itertools
from collections.abc import Iterable


# https://stackoverflow.com/a/66733795
# original autho Seanny123, CC BY-SA 4.0
def compare_ast(node1, node2) -> bool:
    if type(node1) is not type(node2):
        return False

    if isinstance(node1, ast.AST):
        for k, v in vars(node1).items():
            if k in {"lineno", "end_lineno", "col_offset", "end_col_offset", "ctx"}:
                continue
            if not compare_ast(v, getattr(node2, k)):
                return False
        return True

    elif isinstance(node1, list) and isinstance(node2, list):
        return all(compare_ast(n1, n2) for n1, n2 in itertools.zip_longest(node1, node2))
    else:
        return node1 == node2


def compare_two_files(file1: pathlib.Path, file2: pathlib.Path) -> bool:
    with open(file1) as f1:
        a1 = ast.parse(f1.read())
    with open(file2) as f2:
        a2 = ast.parse(f2.read())
    return compare_ast(a1, a2)


def common_suffix_len(a: str, b: str) -> int:
    return len(
        [x for x, y in itertools.takewhile(lambda p: p[0] == p[1], zip(reversed(a), reversed(b)))]
    )


StubFileCompare = tuple[pathlib.Path, pathlib.Path, bool]
StubFileMissing = pathlib.Path


def compare_files(
    files1: Iterable[pathlib.Path],
    files2: Iterable[pathlib.Path],
) -> list[StubFileCompare | StubFileMissing]:
    matches: list[tuple[pathlib.Path, pathlib.Path]] = []
    mismatches: list[pathlib] = []

    files2_remaining = set(files2)
    files1 = sorted(files1, key=lambda p: len(str(p)), reverse=True)
    for f1 in files1:
        best = max(
            files2_remaining, key=lambda f2: common_suffix_len(str(f1), str(f2)), default=None
        )
        if best is not None:
            matches.append((f1, best))
            files2_remaining.remove(best)
        else:
            mismatches.append(f1)
    mismatches += files2
    return [(f1, f2, compare_two_files(f1, f2)) for f1, f2 in matches] + mismatches


def compare_directories(
    dir1: pathlib.Path,
    dir2: pathlib.Path,
) -> list[StubFileCompare | StubFileMissing]:
    return compare_files(
        dir1.glob("**/*.pyi"),
        dir2.glob("**/*.pyi"),
    )


def compare_paths(
    path1: pathlib.Path,
    path2: pathlib.Path,
) -> list[StubFileCompare | StubFileMissing]:
    if path1.is_dir() and path2.is_dir():
        return compare_directories(path1, path2)
    elif not path1.is_dir() and not path2.is_dir():
        return [(path1, path2, compare_two_files(path1, path2))]
    else:
        raise ValueError("Path must be two directories or two files")


if __name__ == "__main__":
    path1 = pathlib.Path(sys.argv[1])
    path2 = pathlib.Path(sys.argv[2])

    comparison = compare_paths(path1, path2)

    ok = True
    for c in comparison:
        if isinstance(c, tuple):
            p1, p2, same = c
            print("{} {} -> {}".format("✅" if same else "❌", p1, p2))
            ok &= same
        else:
            print(f"❌ {c} -> ?")
            ok = False

    if not ok:
        print(
            "\nStubs are out of date! Compile libmambapy, then run:"
            "\n stubgen -o libmambapy/src/ build/libmambapy/"
        )

    sys.exit(0 if ok else 1)
