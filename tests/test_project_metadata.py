"""Guard the CDT metadata that standalone GCC compilation does not exercise."""
from pathlib import Path
import unittest
from xml.dom import Node, minidom
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[1] / 'PVS4'


class CubeIdeProjectMetadata(unittest.TestCase):
    def test_cdt_storage_version_processing_instruction(self):
        # CDT's storage loader requires doc.getFirstChild() to be a PI.
        document = minidom.parse(str(PROJECT / '.cproject'))
        first = document.firstChild
        self.assertEqual(first.nodeType, Node.PROCESSING_INSTRUCTION_NODE)
        self.assertEqual(first.target, 'fileVersion')
        self.assertEqual(first.data, '4.0.0')
        self.assertEqual(document.documentElement.tagName, 'cproject')

    def test_project_natures_and_builders(self):
        project = ET.parse(PROJECT / '.project').getroot()
        self.assertEqual(project.findtext('name'), 'PVS4')
        self.assertIn('org.eclipse.cdt.core.cnature',
                      [n.text for n in project.findall('natures/nature')])
        self.assertIn('org.eclipse.cdt.managedbuilder.core.genmakebuilder',
                      [n.text for n in project.findall('buildSpec/buildCommand/name')])

    def test_both_build_configurations_have_sources_and_kernel_headers(self):
        root = ET.parse(PROJECT / '.cproject').getroot()
        configs = root.findall("./storageModule[@moduleId='org.eclipse.cdt.core.settings']/cconfiguration")
        self.assertEqual(len(configs), 2)
        names = []
        for config in configs:
            settings = config.find("storageModule[@moduleId='org.eclipse.cdt.core.settings']")
            names.append(settings.get('name'))
            self.assertEqual(settings.get('id'), config.get('id'))
            source_names = {e.get('name') for e in config.findall('.//sourceEntries/entry')}
            self.assertTrue({'Core', 'Drivers', 'Middlewares'} <= source_names)
            paths = {e.get('value') for e in config.findall('.//listOptionValue')}
            self.assertIn('../Middlewares/FreeRTOS/include', paths)
            self.assertIn('../Middlewares/FreeRTOS/portable/GCC/ARM_CM4F', paths)
        self.assertEqual(set(names), {'Debug', 'Release'})

    def test_math_library_uses_cubeide_library_option(self):
        root = ET.parse(PROJECT / '.cproject').getroot()
        linkers = root.findall(".//tool[@superClass='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker']")
        self.assertEqual(len(linkers), 2)
        for linker in linkers:
            for option in linker.findall('option'):
                values = [e.get('value') for e in option.findall('listOptionValue')]
                if 'm' in values:
                    self.assertEqual(option.get('superClass'),
                        'com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.libraries')
            libraries = linker.find("option[@superClass='com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.libraries']")
            self.assertIsNotNone(libraries)
            self.assertIn('m', [e.get('value') for e in libraries.findall('listOptionValue')])


if __name__ == '__main__':
    unittest.main()
