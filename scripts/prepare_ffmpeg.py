"""Resolve a retained FFmpeg LGPL release before the expensive Windows build."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import urllib.request
import zipfile


# Pin the same FFmpeg 8.1.3-34 source revision used before 1.1.47.
# The November 6 daily archive expired under upstream retention policy.
# A verified content-addressed CI cache avoids requiring that tag on every build.
REPOSITORY = "FFmpeg-Builds/FFmpeg-Builds"
RELEASE_TAG = "autobuild-2026-11-10-12-39"
API = f"https://api.github.com/repos/{REPOSITORY}/releases/tags/{RELEASE_TAG}"
ASSET = "ffmpeg-n8.1.3-34-ga35157abb9-win64-lgpl-shared-8.1.zip"
ARCHIVE_SHA256 = "3045c2213ed8cbb05edf0d5a631b4490061de3b7567ec145644156e7c98da2c6"


def github_api_headers():
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "FengYin-build",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def select_asset(release):
    asset = next(a for a in release["assets"] if a["name"] == ASSET)
    if asset.get("digest", "").lower() != "sha256:" + ARCHIVE_SHA256:
        raise ValueError("FFmpeg release does not match the pinned SHA256 digest")
    url = asset["browser_download_url"]
    if url != f"https://github.com/{REPOSITORY}/releases/download/{RELEASE_TAG}/{ASSET}":
        raise ValueError("Unexpected FFmpeg download origin")
    return url, ARCHIVE_SHA256


def verify_archive(path, expected):
    with open(path, "rb") as stream:
        hasher = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
        digest = hasher.hexdigest()
    if digest != expected:
        raise ValueError("FFmpeg SHA256 mismatch; refusing to package")


def resolve_archive(cache):
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / (ARCHIVE_SHA256 + ".zip")
    if archive.is_file():
        try:
            verify_archive(archive, ARCHIVE_SHA256)
            return archive
        except ValueError:
            archive.unlink()
    request = urllib.request.Request(API, headers=github_api_headers())
    with urllib.request.urlopen(request, timeout=60) as response:
        release = json.load(response)
    url, expected = select_asset(release)
    partial = archive.with_suffix(".part")
    try:
        with urllib.request.urlopen(url, timeout=120) as response, partial.open("wb") as out:
            shutil.copyfileobj(response, out)
        verify_archive(partial, expected)
        partial.replace(archive)
    finally:
        partial.unlink(missing_ok=True)
    return archive


def prepare(destination):
    cache = Path(os.environ.get("FENGYIN_FFMPEG_CACHE", Path(__file__).resolve().parents[1] / ".cache/fengyin-ffmpeg"))
    for attempt in range(3):
        try:
            archive = resolve_archive(cache)
            destination.mkdir(parents=True, exist_ok=True)
            with zipfile.ZipFile(archive) as package:
                files = [i for i in package.infolist() if not i.is_dir()]
                if not any(i.filename.endswith("/bin/ffmpeg.exe") for i in files):
                    raise ValueError("FFmpeg executable missing from archive")
                if not any(i.filename.endswith("/bin/ffprobe.exe") for i in files):
                    raise ValueError("FFprobe metadata reader missing from archive")
                names = set()
                for item in files:
                    path = Path(item.filename)
                    if path.parent.name == "bin" or path.name.upper().startswith(("LICENSE", "COPYING", "README")):
                        if path.name.lower() in names:
                            raise ValueError("Duplicate flat FFmpeg archive entry")
                        names.add(path.name.lower())
                        with package.open(item) as src, (destination / path.name).open("wb") as out:
                            shutil.copyfileobj(src, out)
            url = f"https://github.com/{REPOSITORY}/releases/download/{RELEASE_TAG}/{ASSET}"
            (destination / "build-source.json").write_text(json.dumps({"url": url, "sha256": ARCHIVE_SHA256}, indent=2))
            print("FFmpeg verified:", ARCHIVE_SHA256)
            return
        except Exception:
            if attempt == 2:
                raise


if __name__ == "__main__":
    import sys
    prepare(Path(sys.argv[1]))
