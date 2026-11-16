#!/usr/bin/env python3
"""Exact-ROM whole-group data and bounded native export preparation.

This module prepares bytes only. It never opens USB, writes native GameData,
changes global settings or claims RAM/IGM hardware compatibility.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import unicodedata
import zlib

_spec = importlib.util.spec_from_file_location("cheat_group_sources", Path(__file__).with_name("build_cheat_view.py"))
source = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(source)
HEADER = struct.Struct("<4sBBHIIIIHHIII")
GROUP = struct.Struct("<IHHIHHI")
CODE = struct.Struct("<IBBH")
MAX_FILE = 256 * 1024
MALFORMED, FLAGS, ACTIVE, ADDRESS, COUNT = 1, 2, 4, 8, 16
REASONS = MALFORMED | FLAGS | ACTIVE | ADDRESS | COUNT


def require(value, message):
    if not value:
        raise ValueError(message)


def uint(value, limit=0xFFFFFFFF):
    return type(value) is int and 0 <= value <= limit


def address_kind(address):
    if 0 < address < 0x100000:
        return 1
    if 0xF82000 <= address <= 0xF83FFF or 0x1F0000 <= address <= 0x1F1FFF:
        return 2
    return 0


def display_name(text):
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return " ".join("".join(c if 32 <= ord(c) < 127 else " " for c in text).split())[:120] or "Unnamed group"


def parse_group(name, data):
    require(isinstance(name, str) and type(data) is bytes, "group source types")
    identity = hashlib.sha256(name.encode("utf-8") + b"\0" + data).digest()
    group = {"id": int.from_bytes(identity[:4], "little"), "name": display_name(name), "codes": [], "reason": 0}
    if len(data) > source.MAX_GROUP_BYTES:
        group["reason"] = COUNT
        return group
    try:
        rows = [line.strip() for line in data.decode("utf-8-sig").splitlines() if line.strip()]
    except UnicodeError:
        group["reason"] = MALFORMED
        return group
    if not 1 <= len(rows) <= 16:
        group["reason"] = COUNT
        return group
    codes = []
    for row in rows:
        match = source.NATIVE_LINE.fullmatch(row)
        if not match:
            group["reason"] |= MALFORMED
            break
        marker, first, second, third, address, value, description = match.groups()
        try:
            address = int(address, 16)
            value = int(value, 16 if value.lower().startswith("0x") else 10)
            require(uint(address, 0xFFFFFF) and uint(value, 0xFF), "code out of range")
            require(isinstance(json.loads(description), str), "invalid description")
            flags = (int(first), int(second), int(third))
        except (ValueError, OverflowError):
            group["reason"] |= MALFORMED
            break
        kind = address_kind(address)
        if not kind:
            group["reason"] |= ADDRESS
        if flags != (1, 0, 1):
            group["reason"] |= FLAGS
        if marker != "-":
            group["reason"] |= ACTIVE
        codes.append((address, value, kind))
    if not group["reason"] & MALFORMED:
        group["codes"] = codes
    return group


def checked_groups(groups):
    require(isinstance(groups, list) and 1 <= len(groups) <= 1024, "group count outside 1..1024")
    ids = set()
    for group in groups:
        require(type(group) is dict and set(group) == {"id", "name", "codes", "reason"}, "group schema")
        require(uint(group["id"]) and group["id"] not in ids, "duplicate or invalid group id")
        ids.add(group["id"])
        name = group["name"]
        require(isinstance(name, str) and 1 <= len(name) <= 120 and all(32 <= ord(c) < 127 for c in name), "invalid group name")
        reason = group["reason"]
        require(uint(reason, REASONS) and reason & ~REASONS == 0, "invalid held reason")
        codes = group["codes"]
        require(type(codes) is list and len(codes) <= 16 and (codes or reason), "whole code count")
        for code in codes:
            require(type(code) is tuple and len(code) == 3, "code schema")
            address, value, kind = code
            require(uint(address, 0xFFFFFF) and uint(value, 255) and uint(kind, 2), "code fields")
            require(kind == address_kind(address) and (kind or reason & ADDRESS), "code address domain")
        require(not reason & MALFORMED or not codes, "malformed group must not retain a partial group")


def encode_package(key, rom_size, rom_crc, groups):
    require(uint(key) and type(rom_size) is int and 1 <= rom_size <= 8 * 1024 * 1024 and uint(rom_crc), "ROM identity")
    checked_groups(groups)
    codes_offset = HEADER.size + len(groups) * GROUP.size
    strings_offset = codes_offset + sum(len(g["codes"]) for g in groups) * CODE.size
    records, codes, strings = bytearray(), bytearray(), bytearray()
    first = 0
    for group in groups:
        name = group["name"].encode("ascii")
        records.extend(GROUP.pack(group["id"], first, len(group["codes"]), strings_offset + len(strings), len(name), group["reason"], 0))
        for address, value, kind in group["codes"]:
            codes.extend(CODE.pack(address, value, kind, 0))
        strings.extend(name)
        first += len(group["codes"])
    payload = bytes(records + codes + strings)
    length = HEADER.size + len(payload)
    require(length <= MAX_FILE, "group package exceeds 256KiB")
    return HEADER.pack(b"TCG1", 1, GROUP.size, HEADER.size, key, rom_size, rom_crc,
                       zlib.crc32(payload), len(groups), 0, codes_offset, strings_offset, length) + payload


def decode_package(data, key, rom_size, rom_crc):
    require(type(data) is bytes and HEADER.size <= len(data) <= MAX_FILE, "package length")
    magic, version, record_size, header_size, actual_key, size, crc, checksum, count, reserved, codes_offset, strings_offset, length = HEADER.unpack_from(data)
    require((magic, version, record_size, header_size, reserved) == (b"TCG1", 1, GROUP.size, HEADER.size, 0), "package header")
    require(uint(key) and type(rom_size) is int and 1 <= rom_size <= 8 * 1024 * 1024 and uint(rom_crc), "ROM identity")
    require((actual_key, size, crc) == (key, rom_size, rom_crc), "exact ROM identity mismatch")
    require(length == len(data) and 1 <= count <= 1024 and codes_offset == HEADER.size + count * GROUP.size and codes_offset <= strings_offset <= length, "package offsets")
    require(zlib.crc32(data[HEADER.size:]) == checksum, "package payload checksum")
    groups, next_code, next_name = [], 0, strings_offset
    for i in range(count):
        ident, first, code_count, name_offset, name_length, reason, zero = GROUP.unpack_from(data, HEADER.size + i * GROUP.size)
        require(zero == 0 and first == next_code and code_count <= 16 and name_offset == next_name and 1 <= name_length <= 120 and next_name + name_length <= length, "group offsets or reserved")
        require(codes_offset + (first + code_count) * CODE.size <= strings_offset, "code range")
        codes = []
        for j in range(code_count):
            address, value, kind, zero = CODE.unpack_from(data, codes_offset + (first + j) * CODE.size)
            require(zero == 0, "code reserved")
            codes.append((address, value, kind))
        try:
            name = data[name_offset:name_offset + name_length].decode("ascii")
        except UnicodeError as error:
            raise ValueError("group name encoding") from error
        groups.append({"id": ident, "name": name, "codes": codes, "reason": reason})
        next_code += code_count
        next_name += name_length
    require(codes_offset + next_code * CODE.size == strings_offset and next_name == length, "package unclaimed bytes")
    checked_groups(groups)
    return groups


def native_export(groups, observed):
    """Prepare complete retained groups; disabled rows count toward native16 limit.

    Caller must pass the RAM hardware gate and journal/ownership validation
    before installing these bytes. All-off clearing is deliberately unresolved.
    """
    checked_groups(groups)
    require(type(observed) is dict and set(observed) <= {g["id"] for g in groups}, "unknown selection")
    rows = []
    for group in groups:
        if group["id"] not in observed:
            continue
        require(group["reason"] == 0, "held whole group cannot be exported")
        bits = observed[group["id"]]
        require(type(bits) is tuple and len(bits) == len(group["codes"]) and all(type(b) is bool for b in bits), "whole group bits required")
        if not any(bits):
            continue
        require(len(rows) + len(bits) <= 16, "native 16 record slots exceeded")
        # Stock tokenizer recognizes braces/commas even inside quoted labels.
        label = " ".join("".join(c if c.isalnum() or c in " .()-_/+" else " "
                                 for c in group["name"]).split())
        for i, ((address, value, kind), enabled) in enumerate(zip(group["codes"], bits), 1):
            suffix = f" {i}/{len(bits)}" if len(bits) > 1 else ""
            title = (label[:19 - len(suffix)] + suffix) or "Cheat"
            rows.append(f'{{{"+" if enabled else "-"},{address:06X},{value:02X},"{title}"}}\r\n'.encode("ascii"))
    require(rows, "all-off clearing has no hardware proof")
    data = b"".join(rows)
    require(len(data) <= 1024, "native importer 1024 byte limit")
    return data


LEGACY_ROW = re.compile(
    rb'\{\s*([+-])\s*,\s*([0-9A-Fa-f]{1,6})\s*,\s*([0-9A-Fa-f]{1,2})\s*,\s*'
    rb'"([A-Za-z0-9 .()_+/\-]{1,19})"\s*\}'
)


def _legacy_rows(data):
    require(type(data) is bytes and 1 <= len(data) <= 1024, "native text size")
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    require(1 <= len(lines) <= 16, "native record count")
    result = []
    for line in lines:
        match = LEGACY_ROW.fullmatch(line)
        require(match is not None, "unknown native syntax")
        marker, address, value, title = match.groups()
        address, value = int(address, 16), int(value, 16)
        require(address_kind(address) != 0, "unknown native address domain")
        result.append((marker == b"+", address, value, title))
    return result


def reconcile_native(reference, observed):
    """Read sign changes only against a separately byte-proved immutable export.

    This comparison alone does not grant ownership. The caller must first bind
    the immutable reference/profile to this exact ROM and TCG1 package.
    """
    expected, actual = _legacy_rows(reference), _legacy_rows(observed)
    require(len(expected) == len(actual) and all(a[1:] == b[1:] for a, b in zip(expected, actual)),
            "native substantive edit; read-only")
    return tuple(row[0] for row in actual)
