#!/usr/bin/env python3
"""Fail closed unless a Windows x64 PE contains a usable embedded symgen manifest."""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


PE32_PLUS = 0x20B
X64_MACHINE = 0x8664
DESCRIPTOR_MAGIC = 0x52444842444D5953  # "SYMDBHDR" as a little-endian uint64.
MANIFEST_MAGIC = b"SYMGEN\0\0"
MANIFEST_VERSION = 2
MANIFEST_HEADER_SIZE = 72


@dataclass(frozen=True)
class Section:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int

    def contains_rva(self, rva: int) -> bool:
        return self.virtual_address <= rva < self.virtual_address + max(
            self.virtual_size, self.raw_size
        )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def checked_slice(data: bytes, offset: int, size: int, label: str) -> bytes:
    require(offset >= 0 and size >= 0 and offset + size <= len(data), f"{label} is truncated")
    return data[offset : offset + size]


def section_raw_offset(section: Section, rva: int) -> int:
    require(section.contains_rva(rva), f"RVA 0x{rva:x} is outside {section.name}")
    delta = rva - section.virtual_address
    require(delta < section.raw_size, f"RVA 0x{rva:x} has no raw data in {section.name}")
    return section.raw_offset + delta


def verify(path: Path) -> None:
    data = path.read_bytes()
    require(len(data) >= 0x40, "file is too small for a DOS header")
    require(data[:2] == b"MZ", "missing DOS MZ signature")

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    checked_slice(data, pe_offset, 24, "PE header")
    require(data[pe_offset : pe_offset + 4] == b"PE\0\0", "missing PE signature")

    machine, section_count = struct.unpack_from("<HH", data, pe_offset + 4)
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    require(machine == X64_MACHINE, f"expected x64 machine 0x8664, got 0x{machine:04x}")
    require(section_count > 0, "PE contains no sections")

    optional_offset = pe_offset + 24
    checked_slice(data, optional_offset, optional_size, "optional header")
    optional_magic = struct.unpack_from("<H", data, optional_offset)[0]
    require(optional_magic == PE32_PLUS, f"expected PE32+ magic 0x20b, got 0x{optional_magic:x}")

    section_table = optional_offset + optional_size
    checked_slice(data, section_table, section_count * 40, "section table")
    sections: list[Section] = []
    for index in range(section_count):
        offset = section_table + index * 40
        name = data[offset : offset + 8].split(b"\0", 1)[0].decode("ascii", "strict")
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, offset + 8
        )
        sections.append(Section(name, virtual_size, virtual_address, raw_size, raw_offset))

    by_name = {section.name: section for section in sections}
    require(".symdbh" in by_name, "missing .symdbh descriptor section")
    require(".symdb" in by_name, "missing embedded .symdb manifest section")

    descriptor = by_name[".symdbh"]
    require(descriptor.raw_size >= 24, ".symdbh descriptor is smaller than 24 bytes")
    descriptor_bytes = checked_slice(data, descriptor.raw_offset, 24, ".symdbh descriptor")
    magic, manifest_rva, manifest_size = struct.unpack("<QQQ", descriptor_bytes)
    require(magic == DESCRIPTOR_MAGIC, f"invalid .symdbh magic 0x{magic:016x}")
    require(manifest_rva != 0, ".symdbh manifest RVA is zero")
    require(manifest_size >= MANIFEST_HEADER_SIZE, ".symdbh manifest size is zero or truncated")

    manifest_section = by_name[".symdb"]
    require(manifest_section.contains_rva(manifest_rva), ".symdbh does not point into .symdb")
    manifest_offset = section_raw_offset(manifest_section, manifest_rva)
    manifest = checked_slice(data, manifest_offset, manifest_size, ".symdb manifest")

    (
        manifest_magic,
        version,
        compression,
        uncompressed_size,
        compressed_size,
        build_id_size,
        _build_id,
        entry_count,
    ) = struct.unpack_from("<8sIIQQI32sI", manifest)
    require(manifest_magic == MANIFEST_MAGIC, f"invalid .symdb magic {manifest_magic!r}")
    require(version == MANIFEST_VERSION, f"expected symgen version 2, got {version}")
    require(compression in (0, 1), f"unsupported symgen compression {compression}")
    require(build_id_size <= 32, f"invalid build-id size {build_id_size}")
    require(compressed_size <= manifest_size - MANIFEST_HEADER_SIZE, ".symdb payload is truncated")
    require(uncompressed_size > 0, ".symdb uncompressed payload is empty")
    require(entry_count > 0, ".symdb contains no hook symbols")

    print(
        f"verified {path}: x64 PE32+, .symdb RVA=0x{manifest_rva:x}, "
        f"size={manifest_size}, entries={entry_count}, compression={compression}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pe", type=Path, help="PE executable to inspect")
    args = parser.parse_args()
    try:
        verify(args.pe)
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        print(f"symbol-manifest verification failed for {args.pe}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
