"""Use 1.1.43 fixtures for assertions about superseded tuning and gain rules."""
from contextlib import contextmanager
from io import BytesIO
import json
from pathlib import Path
from shutil import copy2
from tempfile import TemporaryDirectory
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/tones-before-144.zip'
TONE_PATH = Path('assets/tone-packages')


def previous_bytes(relative_path):
    path = Path(relative_path)
    with ZipFile(FIXTURE) as archive:
        try:
            return archive.read(path.as_posix())
        except KeyError:
            return (ROOT / path).read_bytes()


def previous_package_bytes(filename):
    return previous_bytes(TONE_PATH / filename)


def previous_package(filename):
    return ZipFile(BytesIO(previous_package_bytes(filename)))


@contextmanager
def previous_gain_workspace():
    """Run retired gain scripts on their original packages in an isolated directory."""
    with TemporaryDirectory() as folder:
        workspace = Path(folder)
        assets = workspace / TONE_PATH
        assets.mkdir(parents=True)
        scripts = workspace / 'scripts'
        scripts.mkdir()
        audit = ROOT / TONE_PATH / 'kong-volume-restore-137-audit.json'
        for target in json.loads(audit.read_text(encoding='utf-8'))['targets']:
            name = target['file']
            (assets / name).write_bytes(previous_package_bytes(name))
        copy2(audit, assets)
        for script in ('restore_kong_gain_137.py', 'apply_kong_headroom_134.py'):
            copy2(ROOT / 'scripts' / script, scripts)
        yield workspace
