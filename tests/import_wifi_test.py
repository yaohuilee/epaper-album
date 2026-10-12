"""Exercise actual credential import with temporary synthetic input only."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("import_wifi", Path(__file__).resolve().parents[1] / "tools/import_wifi.py")
wifi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wifi)


class ImportTests(unittest.TestCase):
    def test_roundtrip_special_characters(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "WIFI.txt"
            source.write_text('测试 AP\n pass"word\\ \n', encoding="utf-8-sig")
            wifi.generate(source, root / "local")
            header = (root / "local/wifi_credentials.h").read_text(encoding="utf-8")
            embedded = json.loads(header.split("EPAPER_PRIVATE_WIFI_JSON ", 1)[1])
            self.assertEqual(json.loads(embedded), json.loads((root / "local/wifi-provision.json").read_text(encoding="utf-8")))
            self.assertEqual(json.loads(embedded)["network"]["password"], ' pass"word\\ ')

    def test_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "WIFI.txt"
            for content in ["one line", "\npassword", "a" * 33 + "\npassword", "ap\nshort", "ap\n" + "z" * 64]:
                source.write_text(content, encoding="utf-8")
                with self.assertRaises(ValueError):
                    wifi.read_credentials(source)


if __name__ == "__main__":
    unittest.main()
