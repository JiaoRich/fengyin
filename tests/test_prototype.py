import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HTML = (ROOT / "prototype" / "index.html").read_text(encoding="utf-8")
JS = (ROOT / "prototype" / "app.js").read_text(encoding="utf-8")


class PrototypeStructureTests(unittest.TestCase):
    def test_all_primary_pages_exist(self):
        for page in ("play", "sounds", "chain", "wind", "audio", "settings"):
            self.assertIn(f'id="page-{page}"', HTML)
            self.assertIn(f'data-page="{page}"', HTML)

    def test_script_references_existing_ids(self):
        html_ids = set(re.findall(r'id="([^"]+)"', HTML))
        # Ignore the second dollar sign in the $$() query-all helper.
        script_ids = set(re.findall(r"(?<!\$)\$\('#([^']+)'\)", JS))
        self.assertEqual(set(), script_ids - html_ids)

    def test_required_first_milestone_controls_exist(self):
        for control_id in (
            "video-file", "play-button", "spectrum", "breath-bar",
            "meter-l", "meter-r", "theme", "license-code", "activate", "scan-swam"
        ):
            self.assertIn(f'id="{control_id}"', HTML)

    def test_trial_entry_and_expiry_lock_exist(self):
        for control_id in ("start-trial", "license-lock", "license-lock-primary", "license-lock-activate"):
            self.assertIn(f'id="{control_id}"', HTML)
        self.assertIn("nativeEvent('startTrial')", JS)
        self.assertIn("renderLicenseState", JS)
        self.assertIn("trialRemainingSeconds", JS)

    def test_only_one_page_starts_active(self):
        active_pages = re.findall(r'<section class="page active"', HTML)
        self.assertEqual(1, len(active_pages))

    def test_video_errors_use_non_blocking_toast(self):
        self.assertIn("video.addEventListener('error'", JS)
        self.assertNotIn("alert(", JS)

    def test_video_can_always_be_selected_and_replaced(self):
        for control_id in ("video-file", "choose-video", "change-video"):
            self.assertIn(f'id="{control_id}"', HTML)
        self.assertIn("videoFileInput.click()", JS)
        self.assertIn("event.target.value = ''", JS)
        self.assertIn("$('#change-video').textContent = '更换视频'", JS)
        self.assertIn(".video-topline,.video-controls{z-index:4}", HTML)

    def test_confirmed_visual_features_are_present(self):
        for element_id in ("theme-atmosphere", "breath-wave", "layout-resizer", "lower-stage", "instrument-picture"):
            self.assertIn(f'id="{element_id}"', HTML)
        for theme in ("spring", "summer", "autumn", "winter", "china-red", "gold", "neon", "minimal"):
            self.assertIn(f'value="{theme}"', HTML)
        self.assertIn("drawThemeAtmosphere", JS)

    def test_sound_panel_starts_at_its_minimum_height(self):
        self.assertIn("minimiseSoundPanelOnStartup", JS)
        self.assertIn("bounds.height - dividerHeight - lowerMinimum", JS)
        self.assertIn("requestAnimationFrame(() => requestAnimationFrame(minimiseSoundPanelOnStartup))", JS)

    def test_audio_settings_are_inline_and_apply_immediately(self):
        for control_id in ("audio-driver-select", "audio-output-select", "audio-rate-select",
                           "audio-buffer-select", "audio-latency-value", "audio-auto-optimize"):
            self.assertIn(f'id="{control_id}"', HTML)
        self.assertIn("nativeEvent('applyAudioSettings'", JS)
        self.assertIn("nativeEvent('requestAudioSettings')", JS)
        self.assertIn("addEventListener('audioSettingsState'", JS)
        self.assertNotIn("document.querySelectorAll('#page-audio button')", JS)

    def test_instrument_artwork_covers_every_swam_family(self):
        for key in (
            "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
            "piccolo-trumpet", "double-bass-trombone", "euphonium", "horn-bb",
            "piccolo", "bass-flute", "bass-clarinet", "english-horn", "contrabassoon",
            "violin", "viola", "cello", "double-bass",
        ):
            self.assertIn(f"'{key}'", JS)
        self.assertIn("showInstrumentArtwork", JS)
        self.assertIn("lowerMinimum", JS)

    def test_live_controls_have_real_behaviour(self):
        self.assertIn('id="recording-manager"', HTML)
        self.assertIn("nativeEvent('showRecordings')", JS)
        self.assertIn('id="wind-status-text">未连接电吹管', HTML)
        self.assertIn('id="tone-style-switcher"', HTML)
        self.assertIn('id="tone-style-prev"', HTML)
        self.assertIn('id="tone-style-next"', HTML)
        self.assertIn('id="tone-style-select"', HTML)
        self.assertIn("toneStyleLibrary", JS)
        self.assertIn('data-library="swam"', HTML)
        self.assertIn('data-library="kong"', HTML)
        self.assertIn("中国民乐 · 空音", HTML)
        self.assertIn("kongPresets", JS)
        self.assertNotIn('id="favorite-current"', HTML)
        self.assertNotIn("fengyin-favorite-instruments-v1", JS)
        self.assertIn("document.body.classList.add('video-playing')", JS)
        self.assertIn("document.body.classList.remove('video-playing')", JS)

    def test_performance_transpose_reverb_and_technique_controls_are_live(self):
        for control_id in ("transpose-key", "transpose-dialog", "performance-reverb", "technique-settings", "technique-dialog"):
            self.assertIn(f'id="{control_id}"', HTML)
        for key_name in ("C调", "降E调", "升F调", "降B调"):
            self.assertIn(key_name, HTML)
        self.assertIn("nativeEvent('setKeyTranspose'", JS)
        self.assertIn("请先将电吹管上的调值设置为 C 调", HTML)
        self.assertNotIn("beginKeyCalibration", JS)
        self.assertIn("nativeEvent('setPerformanceReverb'", JS)
        self.assertIn("nativeEvent('beginTechniqueLearn'", JS)
        self.assertIn("nativeEvent('setTechniqueConfiguration'", JS)
        self.assertIn("techniqueId", JS)
        action_bar = HTML.split('<div class="action-bar glass">', 1)[1].split('</div>\n        </div>', 1)[0]
        self.assertIn('id="transpose-key"', action_bar)
        self.assertIn('id="performance-reverb"', action_bar)
        instrument_heading = HTML.split('<div class="instrument-heading">', 1)[1].split('</div></div>', 1)[0]
        self.assertNotIn('id="transpose-key"', instrument_heading)

    def test_smart_adapter_ui_filters_hardware_and_instrument_techniques(self):
        for control_id in ("adapter-device-name", "adapter-recommendations", "adapter-reconnect",
                           "adapter-techniques", "adapter-advanced-toggle"):
            self.assertIn(f'id="{control_id}"', HTML)
        self.assertIn("state.hasBiteSensor", JS)
        self.assertIn("mergeBackendTechniquePlan", JS)
        self.assertIn("backend.relevant === false", JS)
        self.assertIn("backend.pluginSupported === false", JS)
        self.assertIn("按“电吹管型号＋具体乐器”保存", HTML)

    def test_smart_audio_optimisation_is_user_controllable(self):
        self.assertIn('id="smart-audio"', HTML)
        self.assertIn("nativeEvent('setSmartOptimisation'", JS)
        self.assertIn('class="smart-badge"', HTML)

    def test_instrument_art_uses_independent_images_without_distortion(self):
        self.assertIn('object-fit:contain', HTML)
        self.assertIn("'soprano-sax':'instrument_soprano_sax.png'", JS)
        self.assertIn("picture.src = `../assets/instruments/${artwork}`", JS)
        self.assertIn('id="instrument-picture" alt="当前加载的乐器" hidden', HTML)
        self.assertNotIn('instrument-saxophones.png', JS)
        for filename in ("instrument_soprano_sax.png", "instrument_trumpet.png",
                         "instrument_flute.png", "instrument_violin.png"):
            self.assertTrue((ROOT / "assets" / "instruments" / filename).is_file())

    def test_preset_navigation_waits_for_success(self):
        self.assertIn("presetLoadResult", JS)
        self.assertIn("pluginLoadResult", JS)
        self.assertIn("if (!result.success)", JS)

    def test_saved_presets_can_be_deleted_without_hiding_create_action(self):
        render_body = JS.split("function renderPresets()", 1)[1].split("renderPresets();", 1)[0]
        self.assertIn("innerHTML = create +", render_body)
        self.assertIn("nativeEvent('deletePreset', {index})", JS)
        self.assertIn('class="preset preset-create"', JS)
        self.assertIn('class="preset-delete"', JS)
        self.assertIn('.preset-create{', HTML)

    def test_scanned_plugins_become_editable_preset_catalog(self):
        self.assertIn("availableInstruments.map((instrument,index)", JS)
        self.assertIn("supported ? 'scanned' : 'unsupported'", JS)
        self.assertIn('class="preset-edit"', JS)
        self.assertIn("nativeEvent('editPreset', {index})", JS)
        self.assertIn("nativeEvent('saveCustomPreset'", JS)

    def test_professional_settings_save_a_separate_named_plan(self):
        for control_id in ("tone-expert", "expert-confirm-dialog", "expert-dialog", "preset-name-dialog",
                           "save-expert", "preset-name-input"):
            self.assertIn(f'id="{control_id}"', HTML)
        self.assertIn("保存为我的方案", HTML)
        self.assertIn("nativeEvent('previewCustomTone'", JS)
        self.assertIn("nativeEvent('cancelCustomTone')", JS)
        self.assertNotIn('选择音源系列后，下方直接展示全部乐器', HTML)

    def test_only_one_lower_stage_container_exists(self):
        self.assertEqual(1, HTML.count('id="lower-stage"'))

    def test_html_container_tags_are_balanced(self):
        self.assertEqual(len(re.findall(r'<div(?:\s|>)', HTML)), HTML.count('</div>'))

    def test_plugin_manager_rows_have_explicit_layout(self):
        self.assertIn('.plugin-manager>#instrument-load-state{grid-area:2/2/3/4}', HTML)
        self.assertIn('.plugin-manager>label[for="effect-select"]{grid-area:3/1}', HTML)
        self.assertIn('.plugin-manager>#effect-load-state{grid-area:4/2/5/4}', HTML)
        self.assertEqual(HTML.count('{'), HTML.count('}'))


if __name__ == "__main__":
    unittest.main()
