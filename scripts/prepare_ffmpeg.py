"""Resolve a retained FFmpeg LGPL release before the expensive Windows build."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import urllib.request
import zipfile


API = "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/tags/latest"
ASSET = "ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip"


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
    digest = asset.get("digest", "")
    if not digest.startswith("sha256:") or len(digest) != 71:
        raise ValueError("FFmpeg release has no valid SHA256 digest")
    url = asset["browser_download_url"]
    if not url.startswith("https://github.com/BtbN/FFmpeg-Builds/releases/download/"):
        raise ValueError("Unexpected FFmpeg download origin")
    return url, digest[7:].lower()


def verify_archive(path, expected):
    with open(path, "rb") as stream:
        hasher = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
        digest = hasher.hexdigest()
    if digest != expected:
        raise ValueError("FFmpeg SHA256 mismatch; refusing to package")


def prepare(destination):
    # Resolve URL and hash in one API response. A rolling-release update can cause
    # a mismatch, never an unchecked build: fail early and retry with fresh metadata.
    for attempt in range(3):
        try:
            request = urllib.request.Request(API, headers=github_api_headers())
            with urllib.request.urlopen(request, timeout=60) as response:
                release = json.load(response)
            url, expected = select_asset(release)
            with tempfile.TemporaryDirectory(prefix="fengyin-ffmpeg-") as temp:
                archive = Path(temp) / ASSET
                with urllib.request.urlopen(url, timeout=120) as response, archive.open("wb") as out:
                    shutil.copyfileobj(response, out)
                verify_archive(archive, expected)
                destination.mkdir(parents=True, exist_ok=True)
                with zipfile.ZipFile(archive) as package:
                    # Copy flat binary and notice files only, never extract arbitrary paths.
                    files = [i for i in package.infolist() if not i.is_dir()]
                    if not any(i.filename.endswith("/bin/ffmpeg.exe") for i in files):
                        raise ValueError("FFmpeg executable missing from archive")
                    for item in files:
                        path = Path(item.filename)
                        if path.parent.name == "bin" or path.name.upper().startswith(("LICENSE", "COPYING", "README")):
                            with package.open(item) as src, (destination / path.name).open("wb") as out:
                                shutil.copyfileobj(src, out)
                (destination / "build-source.json").write_text(json.dumps({"url": url, "sha256": expected}, indent=2))
            print("FFmpeg verified:", expected)
            return
        except Exception:
            if attempt == 2:
                raise


if __name__ == "__main__":
    import sys
    prepare(Path(sys.argv[1]))
