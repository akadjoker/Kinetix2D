#!/usr/bin/env python3
"""Export a Kinetix project as a browser bundle on Unix or Windows."""

import argparse
import html
import json
import os
from pathlib import Path, PurePosixPath
import re
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


def make_asset_paths_portable(project: Path, stage: Path) -> None:
    """Replace project-local absolute paths in exported scene data.

    The editor may save absolute paths while a project is being edited. Those
    paths do not exist in Emscripten's virtual filesystem, so a Web export must
    use the same logical paths that the runner resolves below ``assets``.
    """
    asset_root = project / "assets"

    def asset_relative_path(value: str) -> Path | None:
        source = Path(value)
        if not source.is_absolute():
            return None
        try:
            return source.resolve().relative_to(asset_root)
        except ValueError:
            # A copied project can contain an editor path from its original
            # location. It is still portable when the same assets/<path> is
            # present in this project; use that local copy instead.
            parts = source.parts
            for index in range(len(parts) - 1, -1, -1):
                if parts[index] == "assets":
                    relative = Path(*parts[index + 1:])
                    if relative and (asset_root / relative).is_file():
                        return relative
                    break
            return None

    for path in stage.rglob("*"):
        if path.suffix not in {".k2dscene", ".k2dprefab", ".k2dproj"}:
            continue
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue

        changed = False

        def rewrite(value: object) -> object:
            nonlocal changed
            if isinstance(value, dict):
                return {key: rewrite(item) for key, item in value.items()}
            if isinstance(value, list):
                return [rewrite(item) for item in value]
            if not isinstance(value, str):
                return value

            relative = asset_relative_path(value)
            if relative is None:
                return value
            changed = True
            return relative.as_posix()

        portable = rewrite(document)
        if changed:
            path.write_text(json.dumps(portable, indent=2) + "\n", encoding="utf-8")


def used_asset_scripts(project: Path, stage: Path, startup_scene: PurePosixPath) -> list[str]:
    """Return the ZenScript files actually referenced by exported content.

    Starting at the exported scene, follow prefabs and literal scene/prefab
    paths used by its scripts. Source files outside ``assets`` are deliberately
    rejected: they cannot be part of a portable Web export.
    """
    asset_root = project / "assets"
    used: set[str] = set()
    pending_documents = [startup_scene]
    seen_documents: set[PurePosixPath] = set()
    scanned_scripts: set[Path] = set()

    def resolve_resource(path: str) -> Path | None:
        raw = Path(path)
        if raw.is_absolute():
            return raw if raw.is_file() else None
        logical = Path(path.removeprefix("assets/"))
        for candidate in (project / raw, asset_root / logical):
            if candidate.is_file():
                return candidate
        return None

    def queue_document(path: str) -> None:
        resource = resolve_resource(path)
        if not resource or resource.suffix not in {".k2dscene", ".k2dprefab"}:
            return
        try:
            pending_documents.append(PurePosixPath(resource.relative_to(project).as_posix()))
        except ValueError as error:
            raise RuntimeError(f"Referenced scene/prefab is outside the project: {path}") from error

    def collect(value: object) -> None:
        if isinstance(value, dict):
            if value.get("type") == "ZenScript":
                data = value.get("data")
                path = data.get("path") if isinstance(data, dict) else None
                if isinstance(path, str) and path:
                    source = Path(path)
                    if not source.is_absolute():
                        source = asset_root / path.removeprefix("assets/")
                    try:
                        relative = source.resolve().relative_to(asset_root)
                    except ValueError as error:
                        raise RuntimeError(
                            f"ZenScript must be inside project/assets: {path}"
                        ) from error
                    if relative.suffix != ".py" or not source.is_file():
                        raise RuntimeError(f"ZenScript was not found: {path}")
                    used.add((Path("assets") / relative).as_posix())
                    scanned_scripts.add(source)
            for item in value.values():
                collect(item)
        elif isinstance(value, list):
            for item in value:
                collect(item)

    while pending_documents or scanned_scripts:
        while pending_documents:
            relative = pending_documents.pop()
            if relative in seen_documents:
                continue
            seen_documents.add(relative)
            path = stage / relative
            if not path.is_file():
                raise RuntimeError(f"Referenced scene/prefab was not found: {relative}")
            if path.suffix not in {".k2dscene", ".k2dprefab"}:
                continue
            try:
                collect(json.loads(path.read_text(encoding="utf-8")))
            except json.JSONDecodeError as error:
                raise RuntimeError(f"Invalid scene data: {relative}") from error

        scripts_to_scan = list(scanned_scripts)
        scanned_scripts.clear()
        for script in scripts_to_scan:
            for reference in re.findall(r"['\"]([^'\"]+\.(?:k2dscene|k2dprefab))['\"]", script.read_text(encoding="utf-8")):
                queue_document(reference)

    return sorted(used)


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


def available_server_port(preferred: int) -> int:
    """Return the preferred loopback port, or a free ephemeral port.

    Run Web leaves its development server alive so the browser can keep
    loading assets. A later Run Web must therefore not mistake that old server
    for its newly exported game.
    """
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        try:
            probe.bind(("127.0.0.1", preferred))
        except OSError:
            probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def parent_is_running(pid: int) -> bool:
    if pid <= 0:
        return True
    if os.name == "nt":
        import ctypes

        synchronize = 0x00100000
        wait_timeout = 0x00000102
        process = ctypes.windll.kernel32.OpenProcess(synchronize, False, pid)
        if not process:
            return False
        try:
            return ctypes.windll.kernel32.WaitForSingleObject(process, 0) == wait_timeout
        finally:
            ctypes.windll.kernel32.CloseHandle(process)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def wait_for_server_exit(server: subprocess.Popen, parent_pid: int) -> int:
    while server.poll() is None:
        if not parent_is_running(parent_pid):
            server.terminate()
            return server.wait()
        time.sleep(0.2)
    return server.returncode


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
    parser.add_argument("--parent-pid", type=int, default=0,
                        help="Stop the development server once this editor process exits.")
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
        make_asset_paths_portable(project, stage)
        bytecode_dir = stage / ".k2d" / "web"
        bytecode_dir.mkdir(parents=True, exist_ok=True)
        scripts = used_asset_scripts(project, stage, normalized_scene)
        if scripts:
            subprocess.run(
                [
                    str(tool_path(root, "k2d_scriptc")),
                    "--bundle",
                    str(bytecode_dir / "scripts.zbc"),
                    str(bytecode_dir / "scripts.json"),
                    *scripts,
                ],
                # Source files are intentionally left out of the staged Web
                # filesystem, so compile them from the original project.
                cwd=project,
                check=True,
            )
            # Script paths in scenes are resolved from the project's assets
            # directory (for example, ``scripts/player.py``).  The compiler
            # receives ``assets/...`` paths so it can read from the staged
            # working directory; rewrite only its manifest back to the
            # runtime-facing logical paths.
            manifest_path = bytecode_dir / "scripts.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            for script in manifest.get("scripts", []):
                script_path = script.get("path", "")
                if script_path.startswith("assets/"):
                    script["path"] = script_path[len("assets/"):]
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
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
        if not parent_is_running(args.parent_pid):
            return 0
        port = available_server_port(args.port)
        server = subprocess.Popen(
            [str(tool_path(root, "k2d_webserver")), str(output), str(port)]
        )
        try:
            url = f"http://127.0.0.1:{port}/"
            wait_for_server(server, port)
            open_default_browser(url)
            print(f"Opened default browser: {url}")
            return wait_for_server_exit(server, args.parent_pid)
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
