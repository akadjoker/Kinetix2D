#!/usr/bin/env python3
"""Export a Kinetix project as a browser bundle on Unix or Windows."""

import argparse
import html
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def tool_path(root: Path, name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    path = root / "bin" / f"{name}{suffix}"
    if not path.is_file():
        raise RuntimeError(f"Required tool was not found: {path}")
    return path


def copy_project(project: Path, stage: Path) -> None:
    def ignored(directory: str, names: list[str]) -> set[str]:
        relative = Path(directory).relative_to(project)
        result = {name for name in names if name.endswith(".py")}
        if relative == Path(".k2d") and "web" in names:
            result.add("web")
        return result

    shutil.copytree(project, stage, ignore=ignored, dirs_exist_ok=True)


def file_packager() -> list[str]:
    emcc = shutil.which("emcc")
    if not emcc:
        raise RuntimeError("emcc was not found. Activate the Emscripten SDK first.")
    tools = Path(emcc).resolve().parent / "tools"
    python_tool = tools / "file_packager.py"
    if python_tool.is_file():
        return [sys.executable, str(python_tool)]
    native_tool = tools / "file_packager"
    if native_tool.is_file():
        return [str(native_tool)]
    raise RuntimeError("Emscripten file_packager was not found next to emcc.")


def wait_for_server(server: subprocess.Popen, port: int) -> None:
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("The local Web server stopped before it was ready.")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.15):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"The local Web server did not open port {port}.")


def open_default_browser(url: str) -> None:
    if os.name == "nt":
        os.startfile(url)  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", url])
    else:
        subprocess.Popen(["xdg-open", url])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--scene", default="")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()

    root = Path(
        os.environ.get("K2D_SOURCE_ROOT", Path(__file__).resolve().parent.parent)
    ).resolve()
    if not (root / "CMakeLists.txt").is_file():
        raise RuntimeError("K2D source root was not found. Set K2D_SOURCE_ROOT.")
    project = args.project.resolve()
    project_file = project / "project.k2dproj"
    if not project_file.is_file():
        raise RuntimeError("Project directory must contain project.k2dproj.")

    metadata = json.loads(project_file.read_text(encoding="utf-8"))
    scene = args.scene or metadata.get("startupScene", "")
    normalized_scene = PurePosixPath(scene)
    if (
        not scene
        or normalized_scene.is_absolute()
        or ".." in normalized_scene.parts
        or "\\" in scene
    ):
        raise RuntimeError("Scene must be a relative path inside the project.")
    if not (project / normalized_scene).is_file():
        raise RuntimeError(f"Scene was not found: {scene}")

    template = root / "bin" / "web" / "_template"
    build = root / "build-web"
    if (
        args.rebuild
        or not (template / "k2d_runner.js").is_file()
        or not (template / "k2d_runner.wasm").is_file()
    ):
        subprocess.run(
            [
                "emcmake",
                "cmake",
                "-S",
                str(root),
                "-B",
                str(build),
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DK2D_RUNTIME_OUTPUT_DIRECTORY={template}",
            ],
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", str(build), "--target", "k2d_runner", "--parallel"],
            check=True,
        )

    output = project / ".k2d" / "web"
    output.mkdir(parents=True, exist_ok=True)
    for name in (
        "k2d_runner.js",
        "k2d_runner.wasm",
        "assets.js",
        "assets.data",
        "index.html",
        "scripts.zbc",
        "scripts.json",
    ):
        (output / name).unlink(missing_ok=True)
    shutil.copy2(template / "k2d_runner.js", output / "k2d_runner.js")
    shutil.copy2(template / "k2d_runner.wasm", output / "k2d_runner.wasm")

    with tempfile.TemporaryDirectory(prefix="k2d-web-stage-") as temporary:
        stage = Path(temporary)
        copy_project(project, stage)
        bytecode_dir = stage / ".k2d" / "web"
        bytecode_dir.mkdir(parents=True, exist_ok=True)
        scripts_root = project / "assets" / "scripts"
        scripts = []
        if scripts_root.is_dir():
            scripts = sorted(
                path.relative_to(project).as_posix()
                for path in scripts_root.rglob("*.py")
            )
        if scripts:
            subprocess.run(
                [
                    str(tool_path(root, "k2d_scriptc")),
                    "--bundle",
                    str(bytecode_dir / "scripts.zbc"),
                    str(bytecode_dir / "scripts.json"),
                    *scripts,
                ],
                cwd=project,
                check=True,
            )
        subprocess.run(
            [
                *file_packager(),
                str(output / "assets.data"),
                "--preload",
                f"{stage}@/project",
                f"--js-output={output / 'assets.js'}",
                "--export-name=Module",
                "--no-node",
                "--quiet",
            ],
            check=True,
        )

    title = html.escape(metadata.get("name") or project.name)
    scene_html = html.escape(normalized_scene.as_posix())
    (output / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">\n'
        f"<title>{title}</title><style>html,body,canvas{{margin:0;width:100%;height:100%;background:#111}}canvas{{display:block;outline:0}}</style>\n"
        f"<canvas id=\"canvas\" tabindex=\"0\"></canvas><script>var Module={{canvas:document.getElementById('canvas'),arguments:['/project/{scene_html}','/project/project.k2dproj']}};</script>\n"
        '<script src="assets.js"></script><script async src="k2d_runner.js"></script>',
        encoding="utf-8",
    )
    print(f"Exported Web game: {output / 'index.html'}")

    if args.run:
        server = subprocess.Popen(
            [str(tool_path(root, "k2d_webserver")), str(output), str(args.port)]
        )
        try:
            url = f"http://127.0.0.1:{args.port}/"
            wait_for_server(server, args.port)
            open_default_browser(url)
            print(f"Opened default browser: {url}")
            return server.wait()
        except KeyboardInterrupt:
            server.terminate()
            return server.wait()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Web export failed: {error}", file=sys.stderr)
        raise SystemExit(1)
