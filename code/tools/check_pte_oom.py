#!/usr/bin/env python3
"""Run the allocation-failure regression plus Lab4 tests in a temporary copy."""

import os
from pathlib import Path
import runpy
import shlex
import shutil
import subprocess
import sys
import tempfile


def main():
    source = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="lab4-pte-oom-") as temporary:
        root = Path(temporary) / "code"
        shutil.copytree(
            source, root,
            ignore=shutil.ignore_patterns(
                ".git", "obj", "bin", "__pycache__", "*.pyc", ".qemu.out", ".gdb.in"
            ),
        )
        # Reuse the supplied function-boundary injector and unchanged tests.
        old_argv = sys.argv
        try:
            sys.argv = [str(source / "tools/inject.py"), str(root)]
            injector = runpy.run_path(str(source / "tools/inject.py"))
        finally:
            sys.argv = old_argv
        shutil.copyfile(source / "tools/test_pte_oom.c", root / "kern/test/test_pte_oom.c")
        (root / "kern/test/test_pte_oom.h").write_text(
            "void test_pte_oom(void);\n", encoding="utf-8"
        )
        pmm = str(root / "kern/mm/pmm.c")
        injector["insert_include"](pmm, "test_pte_oom.h", "pmm.h")
        injector["insert_at_function_end"](pmm, "pmm_init", "test_pte_oom();")

        build = subprocess.run(
            ["make", "-j2"], cwd=root, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        if build.returncode:
            print(build.stdout)
            return build.returncode
        command = shlex.split(os.environ.get("QEMU", "qemu-system-riscv64")) + [
            "-machine", "virt", "-nographic", "-bios", "default",
            "-kernel", "bin/ucore.img", "-monitor", "none",
        ]
        try:
            result = subprocess.run(
                command, cwd=root, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=8,
            )
            output = result.stdout
        except subprocess.TimeoutExpired as error:
            # 兼容停留在监控器的模板版本；下方仍须检查所有测试标记。
            output = error.stdout or b""
        text = output.decode("utf-8", errors="replace")
        expected = [
            "[Test PMM] PASS", "[Test PMM allocation failure] PASS",
            "[Test Process Create] PASS", "[Test Schedule] PASS",
            'this initproc, pid = 1, name = "init"',
            'To U: "Hello world!!".', 'To U: "en.., Bye, Bye. :)"',
        ]
        missing = [marker for marker in expected if marker not in text]
        if missing:
            print(text)
            print("FAIL: missing output: " + ", ".join(missing))
            return 1
        for line in text.splitlines():
            if line.startswith("[Test "):
                print(line)
        return 0


if __name__ == "__main__":
    sys.exit(main())
