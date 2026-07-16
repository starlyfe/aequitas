#!/usr/bin/env python3
"""Aequitas Homebase — cross-platform build / package / run GUI.

Requires Python 3.9+ with tkinter (included with most desktop Python installs).
Launch via:
  tools/AequitasHome.bat          (Windows)
  tools/AequitasHome.sh           (Linux)
  tools/AequitasHome.command      (macOS double-click)
"""

from __future__ import annotations

import os
import platform
import subprocess
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, scrolledtext, ttk

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent


def read_version() -> str:
    path = REPO_ROOT / "VERSION"
    if not path.exists():
        return "?"
    return path.read_text(encoding="utf-8").strip()


def host_platform() -> str:
    system = platform.system()
    if system == "Windows":
        return "windows"
    if system == "Darwin":
        return "macos"
    return "linux"


def build_script_for_host() -> Path:
    host = host_platform()
    if host == "windows":
        return TOOLS_DIR / "build-windows.ps1"
    if host == "macos":
        return TOOLS_DIR / "build-macos.sh"
    return TOOLS_DIR / "build-linux.sh"


class HomebaseApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(f"Aequitas Homebase — v{read_version()}")
        self.geometry("720x560")
        self.minsize(640, 480)

        self.profile = tk.StringVar(value="release")
        self.do_package = tk.BooleanVar(value=True)
        self.do_run = tk.BooleanVar(value=False)
        self.skip_tests = tk.BooleanVar(value=False)
        self._proc: subprocess.Popen[str] | None = None
        self._building = False

        self._build_ui()
        self._refresh_paths()

    def _build_ui(self) -> None:
        pad = {"padx": 10, "pady": 6}

        header = ttk.Frame(self)
        header.pack(fill=tk.X, **pad)
        ttk.Label(header, text="Aequitas Homebase", font=("Segoe UI", 16, "bold")).pack(anchor=tk.W)
        ttk.Label(
            header,
            text=f"Repo: {REPO_ROOT}    Host: {host_platform()}    VERSION: {read_version()}",
        ).pack(anchor=tk.W)

        opts = ttk.LabelFrame(self, text="Build options")
        opts.pack(fill=tk.X, **pad)

        row = ttk.Frame(opts)
        row.pack(fill=tk.X, padx=8, pady=8)
        ttk.Label(row, text="Profile:").pack(side=tk.LEFT)
        for value, label in (
            ("release", "Release"),
            ("dev", "Development"),
            ("debug", "Debug"),
        ):
            ttk.Radiobutton(row, text=label, value=value, variable=self.profile, command=self._on_profile).pack(
                side=tk.LEFT, padx=8
            )

        checks = ttk.Frame(opts)
        checks.pack(fill=tk.X, padx=8, pady=(0, 8))
        self.chk_package = ttk.Checkbutton(
            checks, text="Package into releases/vX.Y.Z/<platform>/", variable=self.do_package
        )
        self.chk_package.pack(anchor=tk.W)
        ttk.Checkbutton(checks, text="Run aequitas after successful build", variable=self.do_run).pack(anchor=tk.W)
        ttk.Checkbutton(checks, text="Skip tests", variable=self.skip_tests).pack(anchor=tk.W)

        hint = ttk.Label(
            opts,
            text="Development = RelWithDebInfo + AEQUITAS_DEV (extra hooks later). "
            "Release packages by default; Dev/Debug do not unless you check Package.",
            wraplength=680,
        )
        hint.pack(anchor=tk.W, padx=8, pady=(0, 8))

        actions = ttk.Frame(self)
        actions.pack(fill=tk.X, **pad)
        self.btn_build = ttk.Button(actions, text="Build", command=self.start_build)
        self.btn_build.pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(actions, text="Open bin folder", command=self.open_bin).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(actions, text="Open releases folder", command=self.open_releases).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(actions, text="Open repo", command=lambda: self._open_path(REPO_ROOT)).pack(side=tk.LEFT)

        self.path_label = ttk.Label(self, text="", wraplength=680)
        self.path_label.pack(fill=tk.X, padx=10)

        log_frame = ttk.LabelFrame(self, text="Output")
        log_frame.pack(fill=tk.BOTH, expand=True, **pad)
        self.log = scrolledtext.ScrolledText(log_frame, height=18, font=("Consolas", 10), state=tk.DISABLED)
        self.log.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        self._on_profile()

    def _on_profile(self) -> None:
        # Default package only for release; leave user override alone if they already toggled.
        if self.profile.get() == "release":
            self.do_package.set(True)
        else:
            self.do_package.set(False)
        self._refresh_paths()

    def _bin_dir(self) -> Path:
        return REPO_ROOT / "build" / self.profile.get() / "bin"

    def _refresh_paths(self) -> None:
        ver = read_version()
        host = host_platform()
        self.path_label.configure(
            text=f"Bin: {self._bin_dir()}    |    Release out: {REPO_ROOT / 'releases' / f'v{ver}' / host}"
        )

    def append_log(self, text: str) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, text)
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def start_build(self) -> None:
        if self._building:
            messagebox.showinfo("Busy", "A build is already running.")
            return
        script = build_script_for_host()
        if not script.exists():
            messagebox.showerror("Missing script", f"Build script not found:\n{script}")
            return

        args: list[str] = []
        profile = self.profile.get()
        if profile == "debug":
            args.append("--debug")
        elif profile == "dev":
            args.append("--dev")
        # release = default (no flag)

        if self.do_package.get():
            if profile != "release":
                args.append("--package")
        else:
            args.append("--no-package")

        if self.do_run.get():
            args.append("--run")
        if self.skip_tests.get():
            args.append("--no-test")

        self.log.configure(state=tk.NORMAL)
        self.log.delete("1.0", tk.END)
        self.log.configure(state=tk.DISABLED)
        self.append_log(f"$ {script.name} {' '.join(args)}\n\n")
        self._building = True
        self.btn_build.configure(state=tk.DISABLED)

        thread = threading.Thread(target=self._run_build, args=(script, args), daemon=True)
        thread.start()

    def _run_build(self, script: Path, args: list[str]) -> None:
        env = os.environ.copy()
        # Help Windows find Vulkan if the user set it system-wide but not in this process.
        try:
            if host_platform() == "windows":
                cmd = [
                    "powershell",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(script),
                    *args,
                ]
            else:
                cmd = ["bash", str(script), *args]
                # Ensure script is executable.
                script.chmod(script.stat().st_mode | 0o111)

            self._proc = subprocess.Popen(
                cmd,
                cwd=str(REPO_ROOT),
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            assert self._proc.stdout is not None
            for line in self._proc.stdout:
                self.after(0, self.append_log, line)
            code = self._proc.wait()
            self.after(0, self.append_log, f"\n[exit {code}]\n")
            if code == 0:
                self.after(0, lambda: messagebox.showinfo("Done", "Build finished successfully."))
            else:
                self.after(0, lambda: messagebox.showerror("Failed", f"Build exited with code {code}."))
        except Exception as exc:  # noqa: BLE001 - show any launcher failure in the UI
            self.after(0, self.append_log, f"\n[launcher error] {exc}\n")
            self.after(0, lambda: messagebox.showerror("Launcher error", str(exc)))
        finally:
            self._proc = None
            self._building = False
            self.after(0, lambda: self.btn_build.configure(state=tk.NORMAL))
            self.after(0, self._refresh_paths)

    def _open_path(self, path: Path) -> None:
        path.mkdir(parents=True, exist_ok=True)
        if host_platform() == "windows":
            os.startfile(path)  # type: ignore[attr-defined]
        elif host_platform() == "macos":
            subprocess.Popen(["open", str(path)])
        else:
            subprocess.Popen(["xdg-open", str(path)])

    def open_bin(self) -> None:
        self._open_path(self._bin_dir())

    def open_releases(self) -> None:
        self._open_path(REPO_ROOT / "releases" / f"v{read_version()}")


def main() -> None:
    # On macOS, prefer the python.org / Homebrew python that ships Tk.
    app = HomebaseApp()
    app.mainloop()


if __name__ == "__main__":
    main()
