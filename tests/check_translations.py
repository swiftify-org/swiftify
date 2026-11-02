# Swiftify, a spotify player for desktop
# Copyright (C) 2026  shred0s
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Check catalog validity, placeholders, and QML/C++ translation coverage."""
import json
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
translations_header = (root / "src/app/Translations.h").read_text(encoding="utf-8")


def is_language_enabled(flag_name):
    match = re.search(rf"\b{flag_name}\s*=\s*(true|false)\b", translations_header)
    assert match is not None, f"Missing {flag_name} in src/app/Translations.h"
    return match.group(1) == "true"


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate translation: {key}")
        result[key] = value
    return result


# (name, catalog file, number of plural forms, enable flag or None for always on)
languages = [
    ("Russian", "translations/ru.json", 3, "kRussianTranslationEnabled"),
    ("German", "translations/de.json", 2, None),
]

sources = []
literal = r'"(?:[^"\\]|\\.)*"'
for path in list((root / "qml").rglob("*.qml")) + list((root / "src").rglob("*.cpp")):
    text = path.read_text(encoding="utf-8")
    patterns = [rf'\b(?:qsTr|tr)\(\s*({literal})',
                rf'\b(?:qsTranslate|QCoreApplication::translate)\(\s*{literal}\s*,\s*({literal})']
    for pattern in patterns:
        for match in re.finditer(pattern, text):
            sources.append((path.relative_to(root), json.loads(match.group(1))))

for name, catalog_path, plural_forms, flag in languages:
    if flag is not None and not is_language_enabled(flag):
        print(f"Skipped {name} translations (disabled in src/app/Translations.h).")
        continue

    catalog = json.loads((root / catalog_path).read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    for source, translated in catalog.items():
        forms = translated if isinstance(translated, list) else [translated]
        if isinstance(translated, list):
            assert len(forms) == plural_forms, f"{name}: expected {plural_forms} plural forms: {source}"
        for form in forms:
            assert isinstance(form, str) and form.strip(), f"{name}: empty translation: {source}"
            assert sorted(re.findall(r"%(?:n|[1-9]\d*)", source)) == sorted(
                re.findall(r"%(?:n|[1-9]\d*)", form)
            ), f"{name}: placeholders differ: {source}"

    missing = [f"{path}: {source}" for path, source in sources if source not in catalog]
    if missing:
        raise SystemExit(f"Missing {name} translations:\n" + "\n".join(missing))
    print(f"Validated {len(catalog)} {name} translations and QML/C++ coverage.")
