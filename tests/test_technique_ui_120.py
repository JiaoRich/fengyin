from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
JS=(ROOT/'prototype/app.js').read_text(encoding='utf-8')
CSS=(ROOT/'prototype/styles.css').read_text(encoding='utf-8')
HTML=(ROOT/'prototype/index.html').read_text(encoding='utf-8')

class ApprovedTechniqueUI(unittest.TestCase):
    def test_secondary_mapping_actions_and_labels(self):
        render=JS.split('function renderTechniqueMappings(',1)[1].split('function startTechniqueLearning(',1)[0]
        self.assertIn('class="technique-map-actions"',render)
        self.assertIn("mapped ? '重新映射' : '点击映射'",render)
        self.assertIn('>清除映射</button>',render)
        self.assertNotIn('>取消映射</button>',render)
        self.assertNotIn('<p class="recommendation">',render)
        self.assertIn("{off:'关闭',breath:'自动',hardware:'咬嘴控制'}",render)
        self.assertIn("{off:'关闭',breath:'自动',hardware:'按键控制'}",render)

    def test_manual_choice_does_not_start_learning(self):
        handler=JS.split("const button=event.target.closest('[data-technique-mode]')",1)[1].split("addEventListener('techniqueConfigurationResult'",1)[0]
        self.assertNotIn('startTechniqueLearning(',handler)
        self.assertIn('setTechniqueConfiguration',handler)

    def test_three_columns_and_no_persistent_technical_copy(self):
        self.assertIn('grid-template-columns:repeat(3,minmax(0,1fr))',CSS)
        self.assertIn('#technique-list .technique-control{display:block;',CSS)
        self.assertIn('id="adapter-summary" hidden',HTML)
        self.assertIn('id="adapter-inline-hint" hidden',HTML)

if __name__=='__main__':unittest.main()
