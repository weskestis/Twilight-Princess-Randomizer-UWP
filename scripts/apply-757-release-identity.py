"""Advance package and startup identities after the .756 lighting stage."""
import argparse
from pathlib import Path


def version(root):
    for path in (
        "src/dusk/ui/prelaunch.cpp",
        "mods/randomizer/src/session.cpp",
        "platforms/uwp/Package.appxmanifest",
        "src/dusk/xbox_memory_budget.hpp",
    ):
        file = root / path
        data = file.read_text(encoding="utf-8")
        if "1.4.1.756" not in data or "1.4.1.757" in data:
            raise ValueError("Expected .756 identity in " + path)
        file.write_text(data.replace("1.4.1.756", "1.4.1.757"), encoding="utf-8", newline="\n")

    journal = root / "src/dusk/startup_guard.hpp"
    data = journal.read_text(encoding="utf-8")
    previous = 'detail::markerPath = userPath / "xbox-startup-stage-756.txt";'
    history = '{"xbox-startup-stage-755.txt",'
    if data.count(previous) != 1 or data.count(history) != 1:
        raise ValueError("Unexpected .756 startup journal")
    data = data.replace(previous, previous.replace("756", "757"))
    data = data.replace(history, '{"xbox-startup-stage-756.txt", "xbox-startup-stage-755.txt",')
    journal.write_text(data, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    version(parser.parse_args().source)
