import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("prepare_ffmpeg", Path(__file__).resolve().parents[1] / "scripts/prepare_ffmpeg.py")
ffmpeg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ffmpeg)


class FfmpegReleaseTests(unittest.TestCase):
    def setUp(self):
        # Cache/network tests isolate the checked-in archive path.
        patch = mock.patch.object(ffmpeg, "PINNED_PARTS", ())
        patch.start()
        self.addCleanup(patch.stop)

    def test_clean_build_uses_exact_archive_without_network(self):
        parts=tuple(Path(__file__).resolve().parents[1]/"third_party/ffmpeg/pinned"/f"ffmpeg-8.1.3-34-lgpl-shared.zip.{i:03}" for i in range(1,11))
        with tempfile.TemporaryDirectory() as temp, mock.patch.object(ffmpeg,"PINNED_PARTS",parts), mock.patch.object(ffmpeg.urllib.request,"urlopen",side_effect=AssertionError("must not use network")):
            archive=ffmpeg.resolve_archive(Path(temp))
            ffmpeg.verify_archive(archive,ffmpeg.ARCHIVE_SHA256)
            self.assertFalse(archive.with_suffix(".part").exists())

    def test_corrupt_pinned_archive_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            part=Path(temp)/"bad.part";part.write_bytes(b"corrupt")
            with mock.patch.object(ffmpeg,"PINNED_PARTS",(part,)), mock.patch.object(ffmpeg.urllib.request,"urlopen",side_effect=AssertionError("must not use network")):
                with self.assertRaises(ValueError):ffmpeg.resolve_archive(Path(temp)/"cache")
            self.assertFalse(list((Path(temp)/"cache").glob("*.part")))

    def test_missing_pinned_chunk_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            part=Path(temp)/"valid.part";part.write_bytes(b"x"*(8*1024*1024))
            with mock.patch.object(ffmpeg,"PINNED_PARTS",(part,Path(temp)/"absent.part")), mock.patch.object(ffmpeg.urllib.request,"urlopen",side_effect=AssertionError("must not use network")):
                with self.assertRaises(OSError):ffmpeg.resolve_archive(Path(temp)/"cache")
            self.assertFalse(list((Path(temp)/"cache").glob("*.part")))

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
        url = f"https://github.com/{ffmpeg.REPOSITORY}/releases/download/{ffmpeg.RELEASE_TAG}/{ffmpeg.ASSET}"
        asset = {"name": ffmpeg.ASSET, "digest": "sha256:" + ffmpeg.ARCHIVE_SHA256,
                 "browser_download_url": url}
        self.assertEqual(ffmpeg.select_asset({"assets": [asset]}), (url, ffmpeg.ARCHIVE_SHA256))
        with self.assertRaises(ValueError):
            ffmpeg.select_asset({"assets": [dict(asset, browser_download_url=url + ".other")]})
        asset["digest"] = "sha256:" + "a" * 64
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

    def test_verified_cache_survives_expired_upstream_tag(self):
        with tempfile.TemporaryDirectory() as temp:
            cache=Path(temp); content=b"known archive"; digest=hashlib.sha256(content).hexdigest()
            (cache/(digest+".zip")).write_bytes(content)
            with mock.patch.object(ffmpeg,"ARCHIVE_SHA256",digest), mock.patch.object(ffmpeg.urllib.request,"urlopen",side_effect=AssertionError("must not use network")):
                self.assertEqual(ffmpeg.resolve_archive(cache).read_bytes(),content)

    def test_corrupt_cache_not_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            cache=Path(temp); archive=cache/(ffmpeg.ARCHIVE_SHA256+".zip");archive.write_bytes(b"corrupt")
            with mock.patch.object(ffmpeg.urllib.request,"urlopen",side_effect=OSError("offline")):
                with self.assertRaises(OSError):ffmpeg.resolve_archive(cache)
            self.assertFalse(archive.exists())
