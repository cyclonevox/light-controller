#!/usr/bin/env python3
"""检查 HTML 文件名怎么变成 C 符号，以及 pages.map 能否改名。"""
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("embed_pages", ROOT / "web" / "embed_pages.py")
embed = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(embed)


class NamingTests(unittest.TestCase):
    def test_default_appends_page(self):
        self.assertEqual(embed.default_symbol("light.html"), "LIGHT_PAGE")
        self.assertEqual(embed.default_symbol("setup.html"), "SETUP_PAGE")
        self.assertEqual(embed.default_symbol("saved.html"), "SAVED_PAGE")

    def test_default_sanitizes_stem(self):
        self.assertEqual(embed.default_symbol("wifi-setup.html"), "WIFI_SETUP_PAGE")

    def test_map_overrides_default(self):
        with tempfile.TemporaryDirectory() as raw:
            web = Path(raw)
            (web / "light.html").write_text("<p>ok</p>", encoding="utf-8")
            (web / "setup.html").write_text("<p>ok</p>", encoding="utf-8")
            mapping = {"light.html": "MAIN_UI"}
            pages = embed.collect_pages(web, mapping)
            names = {symbol: path.name for symbol, path in pages}
            self.assertEqual(names["MAIN_UI"], "light.html")
            self.assertEqual(names["SETUP_PAGE"], "setup.html")

    def test_collision_is_an_error(self):
        with tempfile.TemporaryDirectory() as raw:
            web = Path(raw)
            (web / "a.html").write_text("a", encoding="utf-8")
            (web / "b.html").write_text("b", encoding="utf-8")
            with self.assertRaises(SystemExit):
                embed.collect_pages(web, {"a.html": "PAGE", "b.html": "PAGE"})


if __name__ == "__main__":
    unittest.main()
