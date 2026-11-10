"""Resolve a retained FFmpeg LGPL release before the expensive Windows build."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import urllib.request
import zipfile


# The 1.1.44 installer used the November 6 build. Upstream's "latest"
# ZIP repacks these same files with a different root folder name.
REPOSITORY = "FFmpeg-Builds/FFmpeg-Builds"
RELEASE_TAG = "autobuild-2026-11-06-12-50"
API = f"https://api.github.com/repos/{REPOSITORY}/releases/tags/{RELEASE_TAG}"
ASSET = "ffmpeg-n8.1.3-34-ga35157abb9-win64-lgpl-shared-8.1.zip"
ARCHIVE_SHA256 = "08897c3d53909d8fde6bd5eb9324e7ca41324890d2b03e22c2f9f31025789ee2"


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


def prepare(destination):
    # Verify the retained release metadata and archive before extracting files.
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
                    if not any(i.filename.endswith("/bin/ffprobe.exe") for i in files):
                        raise ValueError("FFprobe metadata reader missing from archive")
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
