import importlib.util, pathlib, tempfile, subprocess, unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
PATH=ROOT/'verification/runners/g5/ur0/source_identity.py'

class SourceIdentityTest(unittest.TestCase):
    def test_dirty_and_new_source_bytes_are_bound_without_output_recursion(self):
        self.assertTrue(PATH.is_file(),'full source identity implementation unavailable')
        spec=importlib.util.spec_from_file_location('ur0_source_identity',PATH);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temporary:
            p=pathlib.Path(temporary);subprocess.run(['git','init','-q',str(p)],check=True)
            (p/'a.cpp').write_text('one');subprocess.run(['git','-C',str(p),'add','a.cpp'],check=True)
            a=module.identity(p);(p/'a.cpp').write_text('two');b=module.identity(p)
            self.assertNotEqual(a['source_tree_sha256'],b['source_tree_sha256'])
            (p/'b.cpp').write_text('new');c=module.identity(p);self.assertNotEqual(c['source_tree_sha256'],b['source_tree_sha256'])
            output=p/'.aegis/results/G5-UR0b/result.json';output.parent.mkdir(parents=True);output.write_text('{}')
            self.assertEqual(c['source_tree_sha256'],module.identity(p)['source_tree_sha256'])

if __name__=='__main__':unittest.main()
