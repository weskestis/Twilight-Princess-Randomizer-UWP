#!/usr/bin/env python3
"""Reject duplicate Randomizer choices and hidden Xbox feature settings."""

from __future__ import annotations

import ast
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


REQUIRED_SETTINGS = (
    "Enemy Randomization",
    "Boss Souls",
    "Enemy Souls",
    "Enemy First-Defeat Checks",
    "Pots",
    "Pumpkins",
    "Seed-Aware Junk",
)


def scalar(text: str) -> str:
    text = text.strip()
    if text.startswith(('"', "'")):
        try:
            value = ast.literal_eval(text)
        except (SyntaxError, ValueError) as error:
            raise ValueError(f"invalid quoted YAML scalar {text!r}") from error
        if not isinstance(value, str):
            raise ValueError(f"non-string YAML option {text!r}")
        return value
    return text


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_randomizer_settings.py SETTINGS_LIST_YAML RANDO_CONFIG_CPP")
        return 2

    yaml_path = Path(sys.argv[1])
    ui_path = Path(sys.argv[2])
    settings: list[str] = []
    options: dict[str, list[str]] = defaultdict(list)
    current: str | None = None
    in_options = False

    for line_number, line in enumerate(yaml_path.read_text(encoding="utf-8").splitlines(), 1):
        setting_match = re.match(r"^- Name:\s*(.+?)\s*$", line)
        if setting_match:
            current = scalar(setting_match.group(1))
            settings.append(current)
            in_options = False
            continue
        if current is None:
            continue
        if line == "  Options:":
            in_options = True
            continue
        if in_options and line.startswith("    - "):
            option_match = re.match(r"^    -\s+(.+?):(?:\s|$)", line)
            if not option_match:
                print(f"{yaml_path}:{line_number}: could not parse option label")
                return 1
            options[current].append(scalar(option_match.group(1)))
            continue
        if in_options and line and not line.startswith("    "):
            in_options = False

    errors: list[str] = []
    for name, count in Counter(settings).items():
        if count != 1:
            errors.append(f"setting {name!r} appears {count} times")
    for setting, choices in options.items():
        for choice, count in Counter(choices).items():
            if count != 1:
                errors.append(f"option {choice!r} appears {count} times in {setting!r}")

    ui_source = ui_path.read_text(encoding="utf-8")
    for name in REQUIRED_SETTINGS:
        if settings.count(name) != 1:
            errors.append(f"required setting {name!r} is missing from the schema")
        marker = f'add_select_setting(leftPane, "{name}");'
        if ui_source.count(marker) != 1:
            errors.append(f"required setting {name!r} is not listed exactly once in the UI")

    if errors:
        for error in errors:
            print(f"error: {error}")
        return 1

    print(
        f"verified {len(settings)} settings with no duplicate choices; "
        f"all {len(REQUIRED_SETTINGS)} restored Xbox rows are visible"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
