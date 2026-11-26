Pinned build dependency, not a new FFmpeg version.
Original: https://github.com/FFmpeg-Builds/FFmpeg-Builds/releases/download/autobuild-2026-11-10-12-39/ffmpeg-n8.1.3-34-ga35157abb9-win64-lgpl-shared-8.1.zip
SHA256: 3045c2213ed8cbb05edf0d5a631b4490061de3b7567ec145644156e7c98da2c6
Identical archive and runtime files to FengYin 1.1.47 build290.
The upstream daily tag expired. Retaining the verified dependency makes clean,
independent-branch builds reproducible without relying on branch-scoped CI cache.
Archive includes upstream licenses; installed files continue to be selected by
scripts/prepare_ffmpeg.py with identical rules. LGPL shared, not GPL builds.
Source revision: a35157abb9; FFmpeg source https://ffmpeg.org/
Build scripts/patches: https://github.com/FFmpeg-Builds/FFmpeg-Builds

The exact ZIP is stored in ten 8-MiB transport chunks (.001 to .010).
The build concatenates them and verifies the ORIGINAL full SHA256 before use.
