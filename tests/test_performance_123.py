from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class Performance123(unittest.TestCase):
    def test_audio_guard_is_scoped_before_any_callback_work(self):
        source=(ROOT/'native/Source/PluginHostEngine.cpp').read_text(encoding='utf-8')
        block=source.split('void RecordingAudioProcessorPlayer::audioDeviceIOCallbackWithContext',1)[1].split('juce::String PluginHostEngine::takeStageTimingReport',1)[0]
        self.assertLess(block.index('juce::ScopedNoDenormals'),block.index('const auto nowSeconds'))
        self.assertNotIn('setParameter',block)

    def test_capture_geometry_cache_is_used_without_format_changes(self):
        source=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('CaptureFrameComposer composer;',source)
        self.assertIn('composer.render(mapped.pData,mapped.RowPitch',source)
        self.assertIn('MF_MT_AVG_BITRATE,8000000',source)
        self.assertIn('MF_MT_AUDIO_AVG_BYTES_PER_SECOND,24000',source)
        self.assertIn('1920.0/',source)
        self.assertIn('1080.0/',source)

    def test_recorder_reports_selected_encoder_only_when_diagnostics_enabled(self):
        source=(ROOT/'native/ScreenRecorder/main.cpp').read_text(encoding='utf-8')
        self.assertIn('FENGYIN_AUDIO_TIMING',source)
        self.assertIn('timing.active()?writer.describeVideoEncoder()',source)
        self.assertIn('GetTransformForStream(video,i',source)
        self.assertIn('MFT_CATEGORY_VIDEO_ENCODER',source)
        self.assertIn('GetItemType(MFT_ENUM_HARDWARE_URL_Attribute',source)
        self.assertNotIn('GetString(MFT_ENUM_HARDWARE_URL_Attribute',source)

if __name__=='__main__':unittest.main()
