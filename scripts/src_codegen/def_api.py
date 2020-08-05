import os

from .codegen_utils import API


def get(path_prefix="./src/"):
    apis = []

    for root, _, files in os.walk(path_prefix):
        for path in files:
            path = os.path.join(root, path)
            if not (path.endswith(".cc") or path.endswith(".cu")):
                continue
            with open(path, "r") as i_f:
                for lineno, line in enumerate(i_f, 1):
                    line = line.strip()

                    if not line.startswith("MNM_REGISTER_GLOBAL(\"mnm."):
                        continue
                    name = line[line.index("(\"") + 2: line.index("\")")]
                    apis.append(API(name=name, path=path, lineno=lineno))

    return apis
