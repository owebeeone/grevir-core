"""Extract Grevir's interrupt-plan bytes from a target compiler object file.

This module reads object metadata only. The wire record inside the section is
validated by the plan decoder, not by this transport adapter.
"""

from __future__ import annotations

import struct


class ObjectSectionError(ValueError):
    """The object cannot supply exactly one relocation-free plan section."""


def _region(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ObjectSectionError("object section extends outside the file")
    return data[offset : offset + size]


def _cstring(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise ObjectSectionError("invalid object string-table offset")
    end = data.find(b"\0", offset)
    if end < 0:
        raise ObjectSectionError("unterminated object string")
    try:
        return data[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise ObjectSectionError("non-ASCII object section name") from error


def _one(matches: list[tuple[bytes, int]]) -> bytes:
    if not matches:
        raise ObjectSectionError("interrupt plan section is missing")
    if len(matches) != 1:
        raise ObjectSectionError("interrupt plan section is duplicated")
    payload, relocations = matches[0]
    if relocations:
        raise ObjectSectionError("interrupt plan section contains relocations")
    if not payload:
        raise ObjectSectionError("interrupt plan section is empty")
    return payload


def _elf(data: bytes) -> bytes:
    if len(data) < 52 or data[4] not in (1, 2) or data[5] not in (1, 2):
        raise ObjectSectionError("unsupported ELF header")
    width = data[4]
    endian = "<" if data[5] == 1 else ">"
    if width == 1:
        header_size = 52
        _region(data, 0, header_size)
        section_offset = struct.unpack_from(endian + "I", data, 32)[0]
        entry_size, count, names_index = struct.unpack_from(endian + "HHH", data, 46)
        section_format = endian + "IIIIIIIIII"
    else:
        header_size = 64
        _region(data, 0, header_size)
        section_offset = struct.unpack_from(endian + "Q", data, 40)[0]
        entry_size, count, names_index = struct.unpack_from(endian + "HHH", data, 58)
        section_format = endian + "IIQQQQIIQQ"
    if count == 0 or names_index >= count:
        raise ObjectSectionError("unsupported ELF section table")
    if entry_size < struct.calcsize(section_format):
        raise ObjectSectionError("invalid ELF section-header size")
    _region(data, section_offset, entry_size * count)
    sections = [struct.unpack_from(section_format, data, section_offset + i * entry_size)
                for i in range(count)]
    names_header = sections[names_index]
    names = _region(data, names_header[4], names_header[5])
    matches: list[tuple[bytes, int]] = []
    for index, section in enumerate(sections):
        name = _cstring(names, section[0])
        if name != ".grevir_irq_plan":
            continue
        payload = _region(data, section[4], section[5])
        relocations = sum(1 for other in sections
                          if other[1] in (4, 9) and other[7] == index and other[5] != 0)
        matches.append((payload, relocations))
    return _one(matches)


def _mach_o(data: bytes) -> bytes:
    magic = struct.unpack_from("<I", data)[0]
    if magic == 0xFEEDFACF:
        header_size, segment_command, segment_size, section_size = 32, 0x19, 72, 80
        section_count_offset = 64
        section_format = "<QQIIIIIIII"
    elif magic == 0xFEEDFACE:
        header_size, segment_command, segment_size, section_size = 28, 0x1, 56, 68
        section_count_offset = 48
        section_format = "<IIIIIIIII"
    else:
        raise ObjectSectionError("unsupported Mach-O byte order or width")
    _region(data, 0, header_size)
    commands, commands_size = struct.unpack_from("<II", data, 16)
    _region(data, header_size, commands_size)
    cursor = header_size
    matches: list[tuple[bytes, int]] = []
    for _ in range(commands):
        _region(data, cursor, 8)
        command, size = struct.unpack_from("<II", data, cursor)
        if size < 8 or cursor + size > header_size + commands_size:
            raise ObjectSectionError("invalid Mach-O load command")
        if command == segment_command:
            if size < segment_size:
                raise ObjectSectionError("short Mach-O segment command")
            count = struct.unpack_from("<I", data, cursor + section_count_offset)[0]
            if size < segment_size + count * section_size:
                raise ObjectSectionError("short Mach-O section table")
            for index in range(count):
                entry = cursor + segment_size + index * section_size
                name = _region(data, entry, 16).split(b"\0", 1)[0]
                if name != b"__grevir_irq":
                    continue
                if magic == 0xFEEDFACF:
                    _, length, offset, _, _, relocations, _, _, _, _ = struct.unpack_from(
                        section_format, data, entry + 32)
                else:
                    _, length, offset, _, _, relocations, _, _, _ = struct.unpack_from(
                        section_format, data, entry + 32)
                matches.append((_region(data, offset, length), relocations))
        cursor += size
    return _one(matches)


def _coff(data: bytes) -> bytes:
    _region(data, 0, 20)
    machine, count, _, symbols_offset, symbol_count, optional_size, _ = struct.unpack_from(
        "<HHIIIHH", data)
    if machine not in (0x14C, 0x8664, 0xAA64) or count == 0:
        raise ObjectSectionError("unsupported COFF header")
    sections_offset = 20 + optional_size
    _region(data, sections_offset, count * 40)
    strings_offset = symbols_offset + symbol_count * 18
    strings = b""
    if symbols_offset:
        length = struct.unpack_from("<I", _region(data, strings_offset, 4))[0]
        if length < 4:
            raise ObjectSectionError("invalid COFF string table")
        strings = _region(data, strings_offset, length)
    matches: list[tuple[bytes, int]] = []
    for index in range(count):
        section = _region(data, sections_offset + index * 40, 40)
        raw_name = section[:8].split(b"\0", 1)[0]
        if raw_name.startswith(b"/"):
            try:
                name = _cstring(strings, int(raw_name[1:]))
            except ValueError as error:
                raise ObjectSectionError("invalid COFF section-name offset") from error
        else:
            try:
                name = raw_name.decode("ascii")
            except UnicodeDecodeError as error:
                raise ObjectSectionError("non-ASCII COFF section name") from error
        if name != ".grevir_irq_plan":
            continue
        length, offset = struct.unpack_from("<II", section, 16)
        relocations = struct.unpack_from("<H", section, 32)[0]
        matches.append((_region(data, offset, length), relocations))
    return _one(matches)


def extract_interrupt_plan(data: bytes) -> bytes:
    """Return the one plan section or raise ``ObjectSectionError``."""
    if data.startswith(b"\x7fELF"):
        return _elf(data)
    if len(data) >= 4 and struct.unpack_from("<I", data)[0] in (0xFEEDFACE, 0xFEEDFACF):
        return _mach_o(data)
    return _coff(data)
