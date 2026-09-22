#!/usr/bin/env python3
"""Verify packaged PE imports against the exact shipped UWP VCLibs framework."""

from __future__ import annotations

import argparse
import struct
import tempfile
import zipfile
from pathlib import Path


RUNTIME_PREFIXES = ("concrt", "msvcp", "vcamp", "vccorlib", "vcomp", "vcruntime")


class PEImage:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError(f"{path}: missing DOS signature")
        pe_offset = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise ValueError(f"{path}: missing PE signature")
        coff = pe_offset + 4
        section_count = struct.unpack_from("<H", self.data, coff + 2)[0]
        optional_size = struct.unpack_from("<H", self.data, coff + 16)[0]
        optional = coff + 20
        magic = struct.unpack_from("<H", self.data, optional)[0]
        if magic == 0x20B:
            self.pointer_size = 8
            self.ordinal_flag = 1 << 63
            directory_offset = optional + 112
        elif magic == 0x10B:
            self.pointer_size = 4
            self.ordinal_flag = 1 << 31
            directory_offset = optional + 96
        else:
            raise ValueError(f"{path}: unsupported PE optional-header magic {magic:#x}")
        self.directories = [
            struct.unpack_from("<II", self.data, directory_offset + index * 8)
            for index in range(16)
        ]
        section_offset = optional + optional_size
        self.sections: list[tuple[int, int, int, int]] = []
        for index in range(section_count):
            offset = section_offset + index * 40
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, offset + 8
            )
            self.sections.append(
                (virtual_address, max(virtual_size, raw_size), raw_offset, raw_size)
            )

    def offset(self, rva: int) -> int:
        for virtual_address, span, raw_offset, raw_size in self.sections:
            if virtual_address <= rva < virtual_address + span:
                delta = rva - virtual_address
                if delta >= raw_size:
                    raise ValueError(f"{self.path}: RVA {rva:#x} is in a virtual section tail")
                return raw_offset + delta
        if 0 <= rva < len(self.data):
            return rva
        raise ValueError(f"{self.path}: unmapped RVA {rva:#x}")

    def c_string(self, rva: int) -> str:
        offset = self.offset(rva)
        end = self.data.find(b"\0", offset)
        if end < 0:
            raise ValueError(f"{self.path}: unterminated string at RVA {rva:#x}")
        return self.data[offset:end].decode("ascii")

    def imports(self) -> list[tuple[str, str | int]]:
        import_rva, import_size = self.directories[1]
        if import_rva == 0:
            return []
        imports: list[tuple[str, str | int]] = []
        descriptor_offset = self.offset(import_rva)
        descriptor_limit = descriptor_offset + import_size
        while descriptor_offset + 20 <= len(self.data):
            original_thunk, timestamp, forwarder, name_rva, first_thunk = struct.unpack_from(
                "<IIIII", self.data, descriptor_offset
            )
            if not any((original_thunk, timestamp, forwarder, name_rva, first_thunk)):
                break
            if descriptor_offset >= descriptor_limit:
                raise ValueError(f"{self.path}: unterminated import descriptor table")
            dll = self.c_string(name_rva).lower()
            thunk_offset = self.offset(original_thunk or first_thunk)
            while True:
                value = struct.unpack_from(
                    "<Q" if self.pointer_size == 8 else "<I", self.data, thunk_offset
                )[0]
                if value == 0:
                    break
                if value & self.ordinal_flag:
                    symbol: str | int = value & 0xFFFF
                else:
                    name_offset = self.offset(value) + 2
                    end = self.data.find(b"\0", name_offset)
                    if end < 0:
                        raise ValueError(f"{self.path}: unterminated import name")
                    symbol = self.data[name_offset:end].decode("ascii")
                imports.append((dll, symbol))
                thunk_offset += self.pointer_size
            descriptor_offset += 20
        return imports

    def exports(self) -> tuple[set[str], set[int]]:
        export_rva, _ = self.directories[0]
        if export_rva == 0:
            return set(), set()
        offset = self.offset(export_rva)
        base, function_count, name_count, functions_rva, names_rva, _ = struct.unpack_from(
            "<IIIIII", self.data, offset + 16
        )
        names_offset = self.offset(names_rva)
        names = {
            self.c_string(struct.unpack_from("<I", self.data, names_offset + index * 4)[0])
            for index in range(name_count)
        }
        functions_offset = self.offset(functions_rva)
        ordinals = {
            base + index
            for index in range(function_count)
            if struct.unpack_from("<I", self.data, functions_offset + index * 4)[0] != 0
        }
        return names, ordinals


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("payload_root", type=Path)
    parser.add_argument("vclibs_appx", type=Path)
    args = parser.parse_args()

    binaries = sorted(
        path
        for path in args.payload_root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".exe", ".dll"}
    )
    with tempfile.TemporaryDirectory(prefix="tpr-vclibs-") as temporary:
        dependency_root = Path(temporary)
        with zipfile.ZipFile(args.vclibs_appx) as archive:
            archive.extractall(dependency_root)
        dependency_files = {
            path.name.lower(): path
            for path in dependency_root.iterdir()
            if path.is_file() and path.suffix.lower() == ".dll"
        }
        dependency_exports = {
            name: PEImage(path).exports() for name, path in dependency_files.items()
        }

        checked = 0
        used_dependencies: set[str] = set()
        failures: list[str] = []
        for binary in binaries:
            for dll, symbol in PEImage(binary).imports():
                if not dll.startswith(RUNTIME_PREFIXES):
                    continue
                relative = binary.relative_to(args.payload_root)
                if dll not in dependency_exports:
                    failures.append(f"{relative}: dependency does not contain {dll}")
                    continue
                names, ordinals = dependency_exports[dll]
                available = symbol in ordinals if isinstance(symbol, int) else symbol in names
                if not available:
                    failures.append(f"{relative}: {dll} does not export {symbol}")
                checked += 1
                used_dependencies.add(dll)

        if failures:
            raise SystemExit("VCLibs import closure failed:\n  " + "\n  ".join(sorted(set(failures))))
        print(
            f"Verified {checked} VCLibs imports across {len(binaries)} packaged binaries "
            f"and {len(used_dependencies)} runtime DLLs."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
