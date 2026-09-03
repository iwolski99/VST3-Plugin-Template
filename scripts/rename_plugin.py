#!/usr/bin/env python3
"""Rename this template into a new plugin project.

Run once, right after cloning, before you write any DSP:

    python3 scripts/rename_plugin.py "Nightglow" \\
        --company "Iwolski Audio" \\
        --website "https://github.com/you/Nightglow" \\
        --bundle-id "com.iwolskiaudio.nightglow" \\
        --manufacturer-code "Iwol" \\
        --plugin-code "Nglw" \\
        --description "A warm analogue-style saturator"

Only --plugin-code needs to be unique per plugin; reuse the same
--manufacturer-code across everything you ship. Both are exactly 4
characters and the first character of each must be uppercase (VST3 rules -
juce_add_plugin() will refuse to configure otherwise).

What this does:
  - Replaces the token "Foundry" everywhere (class names, the CMake
    project/target name, build_windows.bat, the preset file extension and
    folder name) with your plugin name, in both TitleCase and lowercase forms
    where that distinction matters (e.g. .plugintemplatepreset).
  - Replaces "Your Company" / company website / bundle ID / manufacturer code
    / plugin code / description in CMakeLists.txt.
  - Leaves everything under Source/GUI/ and Source/Params/ParamIDs.h /
    ParameterLayout.* alone beyond the rename above - that's the DSP and
    parameter list you're expected to replace by hand next.

Safe to re-run: it only ever replaces the current name, so running it twice
with the same arguments is a no-op the second time.
"""
import argparse
import pathlib
import re
import sys

OLD_NAME = "Foundry"

TEXT_EXTENSIONS = {".cpp", ".h", ".hpp", ".txt", ".bat", ".sh", ".py", ".md", ".yml", ".yaml"}

SKIP_DIRS = {".git", "build", "Build", "_deps", ".github"}


def is_pascal(s: str) -> bool:
    return bool(re.fullmatch(r"[A-Z][A-Za-z0-9]*", s))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name", help='New plugin name, PascalCase, e.g. "Nightglow"')
    parser.add_argument("--company", default="Your Company")
    parser.add_argument("--website", default="https://example.com")
    parser.add_argument("--bundle-id", default=None, help="defaults to com.yourcompany.<name lowercase>")
    parser.add_argument("--manufacturer-code", required=True, help="4 chars, first uppercase, e.g. Iwol")
    parser.add_argument("--plugin-code", required=True, help="4 chars, first uppercase, unique per plugin")
    parser.add_argument("--description", default="A JUCE VST3 plugin")
    parser.add_argument("--root", default=".", help="repo root (default: current directory)")
    args = parser.parse_args()

    if not is_pascal(args.name):
        print(f"error: plugin name '{args.name}' should be PascalCase, e.g. Nightglow", file=sys.stderr)
        return 1

    for label, code in (("--manufacturer-code", args.manufacturer_code), ("--plugin-code", args.plugin_code)):
        if len(code) != 4 or not code[0].isupper():
            print(f"error: {label} '{code}' must be exactly 4 characters, first uppercase", file=sys.stderr)
            return 1

    bundle_id = args.bundle_id or f"com.yourcompany.{args.name.lower()}"
    root = pathlib.Path(args.root).resolve()

    # --- 1. rename the identifier everywhere in text files -------------------
    changed_files = 0
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        if path.suffix.lower() not in TEXT_EXTENSIONS:
            continue

        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue

        new_text = text.replace(OLD_NAME, args.name)
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            changed_files += 1

    # --- 2. CMakeLists.txt identity fields ------------------------------------
    cmake_path = root / "CMakeLists.txt"
    cmake_text = cmake_path.read_text(encoding="utf-8")
    cmake_text = cmake_text.replace('COMPANY_NAME                "Your Company"',
                                    f'COMPANY_NAME                "{args.company}"')
    cmake_text = cmake_text.replace('COMPANY_WEBSITE             "https://example.com"',
                                    f'COMPANY_WEBSITE             "{args.website}"')
    cmake_text = cmake_text.replace('BUNDLE_ID                   "com.yourcompany.plugintemplate"',
                                    f'BUNDLE_ID                   "{bundle_id}"')
    cmake_text = cmake_text.replace("PLUGIN_MANUFACTURER_CODE    Ycmp",
                                    f"PLUGIN_MANUFACTURER_CODE    {args.manufacturer_code}")
    cmake_text = cmake_text.replace("PLUGIN_CODE                 Tmpl",
                                    f"PLUGIN_CODE                 {args.plugin_code}")
    cmake_text = cmake_text.replace('DESCRIPTION                 "A JUCE VST3 plugin"',
                                    f'DESCRIPTION                 "{args.description}"')
    cmake_path.write_text(cmake_text, encoding="utf-8")

    # --- 3. preset file extension / folder name (lowercase form) -------------
    preset_manager = root / "Source" / "Presets" / "PresetManager.cpp"
    if preset_manager.exists():
        text = preset_manager.read_text(encoding="utf-8")
        text = text.replace(".pluginpreset", f".{args.name.lower()}preset")
        preset_manager.write_text(text, encoding="utf-8")

    print(f"Renamed {OLD_NAME} -> {args.name} in {changed_files} file(s).")
    print(f"Bundle ID:    {bundle_id}")
    print(f"Manufacturer: {args.manufacturer_code}   Plugin code: {args.plugin_code}")
    print()
    print("Next: open Source/GUI/Theme.cpp and pick your own accent colours,")
    print("replace Source/DSP/ExampleEffect.h with your real DSP, and update")
    print("Source/Params/ParamIDs.h + ParameterLayout.cpp for its parameters.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
