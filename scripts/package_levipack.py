import argparse
import json
import re
import sys
import zipfile
from pathlib import Path

VALUE_PATTERN = re.compile(
    r'^\s*inline\s+constexpr\s+std::string_view\s+'
    r'(Name|Author|Description|Version)\s*=\s*"((?:\\.|[^"\\])*)";\s*$'
)
REQUIRED_VALUES = ("Name", "Author", "Description", "Version")


def parse_version(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = VALUE_PATTERN.match(line)
        if match:
            values[match.group(1)] = bytes(
                match.group(2), "utf-8"
            ).decode("unicode_escape")

    missing = [name for name in REQUIRED_VALUES if not values.get(name)]
    if missing:
        raise ValueError("Missing version metadata: " + ", ".join(missing))
    return values


def build_manifest(values: dict[str, str]) -> dict[str, object]:
    return {
        "type": "preload-native",
        "name": "RandomlyThingies",
        "author": "Blurmistredist",
        "description": values["Description"],
        "version": values["Version"],
        "entry": "libRandomlyThingies.so",
        "icon": "icon.png",
        "overwrite_files": [],
        "overwrite_folders": [],
    }


def write_package(
    library: Path,
    icon: Path,
    version_header: Path,
    output: Path,
) -> None:
    for path, label in (
        (library, "Library"),
        (icon, "Icon"),
        (version_header, "Version header"),
    ):
        if not path.is_file():
            raise FileNotFoundError(f"{label} not found: {path}")

    manifest = build_manifest(parse_version(version_header))
    output.parent.mkdir(parents=True, exist_ok=True)

    if output.exists():
        output.unlink()

    manifest_bytes = (
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    ).encode("utf-8")

    with zipfile.ZipFile(
        output, "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9
    ) as archive:
        archive.writestr("manifest.json", manifest_bytes)
        archive.write(library, "libRandomlyThingies.so")
        archive.write(icon, "icon.png")

    with zipfile.ZipFile(output, "r") as archive:
        names = set(archive.namelist())
        expected = {
            "manifest.json",
            "libRandomlyThingies.so",
            "icon.png",
        }

        if names != expected:
            raise RuntimeError(
                f"Unexpected package entries: {sorted(names)}"
            )

        parsed = json.loads(archive.read("manifest.json"))
        if parsed != manifest:
            raise RuntimeError("Manifest verification failed")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--icon", required=True, type=Path)
    parser.add_argument("--version-header", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    try:
        write_package(
            args.library.resolve(),
            args.icon.resolve(),
            args.version_header.resolve(),
            args.output.resolve(),
        )
    except Exception as error:
        print(error, file=sys.stderr)
        return 1

    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
