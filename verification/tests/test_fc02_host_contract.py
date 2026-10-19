from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
class FC02HostBoundary(unittest.TestCase):
 def test_geometry_uses_existing_layout_owner(self):
  self.assertIn('editingGeometry', (ROOT/'runtime/text/include/canvas/text/text_layout.hpp').read_text(encoding="utf-8"))
  self.assertIn('readTextGeometry', (ROOT/'runtime/foundation/include/canvas/runtime/runtime_facade.hpp').read_text(encoding="utf-8"))
 def test_windows_collects_real_ime_result_and_completion(self):
  text=''.join(p.read_text(encoding="utf-8") for p in (ROOT/'apps/minimal_runtime_host').glob('windows*') if p.suffix in ['.cpp','.inl','.hpp'])
  for fact in ['WM_IME_COMPOSITION','GCS_RESULTSTR','ImmNotifyIME']: self.assertIn(fact,text)
 def test_web_collects_platform_composition(self):
  text=''.join(p.read_text(encoding="utf-8") for p in (ROOT/'apps/axiom_canvas_demo/platform/web').glob('*.js'))
  for fact in ['compositionstart','compositionend','beforeinput']: self.assertIn(fact,text)
 def test_text_is_a_runtime_tool(self):
  self.assertIn('kText', (ROOT/'runtime/foundation/include/canvas/runtime/canvas_control_types.hpp').read_text(encoding="utf-8").split('enum class CanvasToolKind')[1].split(';')[0])
if __name__=='__main__':unittest.main()
