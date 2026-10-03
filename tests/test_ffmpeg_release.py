import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("prepare_ffmpeg", Path(__file__).resolve().parents[1] / "scripts/prepare_ffmpeg.py")
ffmpeg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ffmpeg)


class FfmpegReleaseTests(unittest.TestCase):
    def test_api_request_uses_github_token_when_available(self):
        previous = os.environ.get("GITHUB_TOKEN")
        try:
            os.environ["GITHUB_TOKEN"] = "test-token"
            headers = ffmpeg.github_api_headers()
            self.assertEqual(headers["Authorization"], "Bearer test-token")
            self.assertEqual(headers["Accept"], "application/vnd.github+json")
        finally:
            if previous is None:
                os.environ.pop("GITHUB_TOKEN", None)
            else:
                os.environ["GITHUB_TOKEN"] = previous

    def test_requires_exact_lgpl_asset_and_digest(self):
        asset = {"name": ffmpeg.ASSET, "digest": "sha256:" + "a" * 64,
                 "browser_download_url": "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/test.zip"}
        self.assertEqual(ffmpeg.select_asset({"assets": [asset]})[1], "a" * 64)
        asset["digest"] = ""
        with self.assertRaises(ValueError):
            ffmpeg.select_asset({"assets": [asset]})

    def test_corrupted_download_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "archive"
            path.write_bytes(b"test")
            ffmpeg.verify_archive(path, hashlib.sha256(b"test").hexdigest())
            with self.assertRaises(ValueError):
                ffmpeg.verify_archive(path, "0" * 64)

    def test_dependency_preflight_precedes_compile(self):
        script = (Path(__file__).resolve().parents[1] / "scripts/build-windows.ps1").read_text(encoding="utf-8-sig")
        self.assertLess(script.index('prepare_ffmpeg.py'), script.index('cmake -S'))
        self.assertNotIn('autobuild-2026-09-15', script)
