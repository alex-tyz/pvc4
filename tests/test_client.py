import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('compass_client', ROOT / 'client/compass_client.py')
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


class ProtocolCompatibility(unittest.TestCase):
    def test_data(self):
        msg = client.parse_message('DATA -202 0 -22 270 200 0 50')
        self.assertEqual((msg['x'], msg['dir'], msg['scales']), (-202, 270, [200, 0, 50]))

    def test_replies(self):
        self.assertEqual(client.parse_message('OK SCALE 0 100 200')['scales'], [0, 100, 200])
        self.assertEqual(client.parse_message('PONG COMPASS LSM9DS1')['type'], 'PONG')
        self.assertEqual(client.parse_message('ERR SENSOR')['reason'], 'SENSOR')

    def test_invalid(self):
        for text in ['', 'DATA 1 2', 'DATA x 2 3 4 100 100 100']:
            self.assertIsNone(client.parse_message(text))


if __name__ == '__main__':
    unittest.main()
