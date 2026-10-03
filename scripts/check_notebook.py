#!/usr/bin/env python3
"""Execute a Jupyter notebook top to bottom and fail on any error.

Usage: python scripts/check_notebook.py notebooks/oes32_triage_explainer.ipynb [--write]

The notebook runs with its own directory as the working directory, as Jupyter
does. With --write, the freshly executed outputs are saved back to the file.
Exit code 0 means every cell ran without raising an exception.
"""
import argparse
import sys
from pathlib import Path

import nbformat
from nbclient import NotebookClient


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("notebook", type=Path)
    ap.add_argument("--write", action="store_true", help="save executed outputs back to the file")
    ap.add_argument("--timeout", type=int, default=600)
    args = ap.parse_args()

    nb = nbformat.read(args.notebook, as_version=4)
    client = NotebookClient(nb, timeout=args.timeout, kernel_name="python3",
                            resources={"metadata": {"path": str(args.notebook.parent)}})
    client.execute()  # raises CellExecutionError on the first failing cell
    n_code = sum(c.cell_type == "code" for c in nb.cells)
    errors = [o for c in nb.cells if c.cell_type == "code" for o in c.get("outputs", [])
              if o.get("output_type") == "error"]
    if errors:
        print(f"{len(errors)} error output(s) in {args.notebook}", file=sys.stderr)
        return 1
    for c in nb.cells:
        if c.cell_type == "code":
            for o in c.get("outputs", []):
                if o.get("output_type") == "stream" and o.get("name") == "stdout":
                    sys.stdout.write(o["text"])
    if args.write:
        nbformat.write(nb, args.notebook)
    print(f"OK: {args.notebook} executed cleanly ({n_code} code cells)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
