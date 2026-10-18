"""Check local Markdown targets and English/Chinese counterpart coverage."""
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
files = [ROOT / name for name in ("README.md", "CONTRIBUTING.md", "SECURITY.md", "THIRD_PARTY_NOTICES.md")]
files += list((ROOT / "docs").rglob("*.md"))
files += list(ROOT.glob("*.zh-CN.md"))
errors, links = [], 0
for file in files:
    text = file.read_text(encoding="utf-8")
    # Fenced examples are not rendered Markdown links.
    text = re.sub(r"```[^\n]*\n[\s\S]*?```", "", text)
    for target in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", text):
        target = target.strip().strip("<>")
        parsed = urlsplit(target)
        if parsed.scheme or target.startswith(("#", "//")):
            continue
        links += 1
        path = (file.parent / unquote(parsed.path)).resolve()
        if not path.is_relative_to(ROOT) or not path.exists():
            errors.append(f"{file.relative_to(ROOT)}: missing/unsafe target {target}")
    relative = file.relative_to(ROOT)
    if relative.parts[0] == "docs" and "zh-CN" not in relative.parts:
        peer = ROOT / "docs/zh-CN" / file.relative_to(ROOT / "docs")
    elif file.parent == ROOT and not file.name.endswith(".zh-CN.md"):
        peer = file.with_name(file.stem + ".zh-CN.md")
    else:
        continue
    if not peer.exists():
        errors.append(f"{relative}: missing Chinese counterpart {peer.name}")
    elif "简体中文" not in text or "English" not in peer.read_text(encoding="utf-8"):
        errors.append(f"{relative}: missing language navigation")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Documentation checks passed: {len(files)} documents, {links} local links and bilingual counterparts")
