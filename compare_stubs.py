import ast
import sys
from itertools import zip_longest


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
        return all(compare_ast(n1, n2) for n1, n2 in zip_longest(node1, node2))
    else:
        return node1 == node2


if __name__ == "__main__":
    print(sys.argv)

    f1, f2 = sys.argv[1:3]

    with open(f1) as fi:
        a1 = ast.parse(fi.read())

    with open(f2) as fi:
        a2 = ast.parse(fi.read())

    if not compare_ast(a1, a2):
        print("Stubs are different! Please rerun pybind11-stubgen")
        print("CI has these: \n\n")
        with open(f2) as fi:
            print(fi.read())
        sys.exit(1)
