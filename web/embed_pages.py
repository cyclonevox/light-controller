#!/usr/bin/env python3
"""把 web/ 下的 HTML 嵌进 src/web_pages.h，给固件当 PROGMEM 字符串用。

板子上没有文件系统，页面只能烧进程序。默认按文件名命名：
light.html -> LIGHT_PAGE。要改 C 符号名时，在 pages.map 里写
`文件名  符号`。生成结果放 src/，不和这个脚本放在一起。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WEB = Path(__file__).resolve().parent
ROOT = WEB.parent
OUT = ROOT / "src" / "web_pages.h"
MAP_FILE = WEB / "pages.map"

IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def default_symbol(filename: str) -> str:
    stem = Path(filename).stem
    ident = re.sub(r"[^0-9A-Za-z]+", "_", stem).strip("_").upper()
    if not ident:
        raise SystemExit(f"{filename}: empty identifier after default naming")
    if ident[0].isdigit():
        ident = f"PAGE_{ident}"
    if not ident.endswith("_PAGE"):
        ident += "_PAGE"
    return ident


def load_map(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    mapping: dict[str, str] = {}
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            raise SystemExit(f"{path}:{lineno}: expected `filename SYMBOL`")
        filename, symbol = parts
        if not IDENT_RE.match(symbol):
            raise SystemExit(f"{path}:{lineno}: invalid C identifier {symbol!r}")
        if filename in mapping:
            raise SystemExit(f"{path}:{lineno}: duplicate mapping for {filename}")
        mapping[filename] = symbol
    return mapping


def collect_pages(web: Path, mapping: dict[str, str]) -> list[tuple[str, Path]]:
    files = sorted(web.glob("*.html"))
    if not files:
        raise SystemExit(f"no HTML pages in {web}")

    known = {path.name for path in files}
    missing = sorted(set(mapping) - known)
    if missing:
        raise SystemExit("pages.map refers to missing files: " + ", ".join(missing))

    pages: list[tuple[str, Path]] = []
    used: dict[str, str] = {}
    for path in files:
        symbol = mapping.get(path.name, default_symbol(path.name))
        if symbol in used:
            raise SystemExit(f"{path.name} and {used[symbol]} both map to {symbol}")
        used[symbol] = path.name
        pages.append((symbol, path))
    return pages


def render_header(pages: list[tuple[str, Path]]) -> str:
    chunks = [
        "/* 由 web/embed_pages.py 生成：各 HTML 页面的 PROGMEM 字符串。不要手改。 */\n",
        "#pragma once\n\n",
    ]
    for symbol, path in pages:
        html = path.read_text(encoding="utf-8")
        if ")HTML" in html:
            raise SystemExit(f"{path.name} contains )HTML, which would break the string wrapper")
        chunks.append(f'const char {symbol}[] PROGMEM = R"HTML({html})HTML";\n\n')
    return "".join(chunks)


def main() -> None:
    pages = collect_pages(WEB, load_map(MAP_FILE))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(render_header(pages), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except OSError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
